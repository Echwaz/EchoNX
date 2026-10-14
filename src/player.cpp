#include "player.hpp"
#include <stdexcept>
#include <cstring>
#include <cmath>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/channel_layout.h>
}

namespace echo {
namespace {
struct RangeReader {
    Stream stream;std::atomic<bool>* abort;int64_t offset=0,start=-1;std::string block,error;
    static int read(void* opaque,uint8_t* dst,int requested){
        auto& r=*static_cast<RangeReader*>(opaque);
        if(r.abort->load())return AVERROR_EXIT;
        if(r.offset>=r.stream.length)return AVERROR_EOF;
        try{
            if(r.start<0 || r.offset<r.start || r.offset>=r.start+static_cast<int64_t>(r.block.size())){
                int64_t start=r.offset,end=std::min(r.stream.length-1,start+262143);r.start=-1;
                for(int attempt=0;;attempt++){try{auto block=http(r.stream.url+"&range="+std::to_string(start)+"-"+std::to_string(end),"",{},r.abort,r.stream.agent.c_str(),262144);if(block.size()!=size_t(end-start+1))throw std::runtime_error("Incomplete audio range");r.block=std::move(block);r.start=start;r.error.clear();break;}catch(...){if(r.abort->load()||attempt>=2)throw;for(int wait=0;wait<20*(attempt+1)&&!r.abort->load();wait++)SDL_Delay(25);}}
            }
            int count=std::min<int64_t>(requested,r.block.size()-(r.offset-r.start));
            std::memcpy(dst,r.block.data()+(r.offset-r.start),count);r.offset+=count;return count;
        }catch(const std::exception& e){r.error=e.what();return AVERROR(EIO);}
    }
    static int64_t seek(void* opaque,int64_t offset,int whence){
        auto& r=*static_cast<RangeReader*>(opaque);
        if(whence&AVSEEK_SIZE)return r.stream.length;
        whence&=~AVSEEK_FORCE;int64_t next=offset;
        if(whence==SEEK_CUR)next+=r.offset;else if(whence==SEEK_END)next+=r.stream.length;else if(whence!=SEEK_SET)return AVERROR(EINVAL);
        if(next<0 || next>r.stream.length)return AVERROR(EINVAL);
        return r.offset=next;
    }
};
struct Media {
    AVFormatContext* format=nullptr;AVCodecContext* codec=nullptr;AVIOContext* io=nullptr;
    SwrContext* resampler=nullptr;AVFrame* frame=av_frame_alloc();AVPacket* packet=av_packet_alloc();
    ~Media(){av_packet_free(&packet);av_frame_free(&frame);swr_free(&resampler);avcodec_free_context(&codec);if(format)avformat_close_input(&format);if(io){av_freep(&io->buffer);avio_context_free(&io);}}
};
void check(int rc,const char* operation){if(rc<0){char msg[256];av_strerror(rc,msg,sizeof(msg));throw std::runtime_error(std::string(operation)+": "+msg);}}
}
Player::Player(Api& api):api_(api){
    SDL_AudioSpec want{};want.freq=48000;want.format=AUDIO_S16SYS;want.channels=2;want.samples=1024;
    device_=SDL_OpenAudioDevice(nullptr,0,&want,nullptr,0);
    if(!device_)throw std::runtime_error(SDL_GetError());
}
Player::~Player(){stop();if(device_)SDL_CloseAudioDevice(device_);}
void Player::status(const std::string& s){std::lock_guard<std::mutex> l(mutex_);status_=s;}
std::string Player::status()const{std::lock_guard<std::mutex> l(mutex_);return status_;}
void Player::stop(){abort_=true;if(worker_.joinable())worker_.join();SDL_ClearQueuedAudio(device_);SDL_PauseAudioDevice(device_,1);active_=false;done_=false;}
void Player::play(const Song& song){stop();abort_=false;paused_=false;done_=false;seek_=-1;position_=0;duration_=0;active_=true;status("Connecting to YouTube Music");SDL_PauseAudioDevice(device_,0);worker_=std::thread([this,song]{decode(song);});}
void Player::toggle(){if(!active_)return;paused_=!paused_.load();SDL_PauseAudioDevice(device_,paused_?1:0);}
void Player::seek(double s){if(active_ && duration_>0)seek_=std::max(0.0,std::min(s,duration_.load()-0.1));}
void Player::decode(const Song& song){
    try{
        RangeReader reader{api_.resolve(song.id,&abort_),&abort_};Media m;
        auto* buffer=static_cast<uint8_t*>(av_malloc(32768));if(!buffer)throw std::bad_alloc();
        m.io=avio_alloc_context(buffer,32768,0,&reader,RangeReader::read,nullptr,RangeReader::seek);
        if(!m.io){av_free(buffer);throw std::bad_alloc();}
        m.format=avformat_alloc_context();if(!m.format)throw std::bad_alloc();
        m.format->pb=m.io;m.format->flags|=AVFMT_FLAG_CUSTOM_IO;
        m.format->interrupt_callback={[](void* p){return static_cast<std::atomic<bool>*>(p)->load()?1:0;},&abort_};
        check(avformat_open_input(&m.format,nullptr,nullptr,nullptr),"Open audio");
        check(avformat_find_stream_info(m.format,nullptr),"Read audio metadata");
        int track=av_find_best_stream(m.format,AVMEDIA_TYPE_AUDIO,-1,-1,nullptr,0);check(track,"Find audio track");
        const AVCodec* decoder=avcodec_find_decoder(m.format->streams[track]->codecpar->codec_id);
        if(!decoder)throw std::runtime_error("Audio decoder unavailable");
        m.codec=avcodec_alloc_context3(decoder);if(!m.codec)throw std::bad_alloc();
        check(avcodec_parameters_to_context(m.codec,m.format->streams[track]->codecpar),"Configure decoder");
        check(avcodec_open2(m.codec,decoder,nullptr),"Start decoder");
        AVChannelLayout layout=AV_CHANNEL_LAYOUT_STEREO;
        check(swr_alloc_set_opts2(&m.resampler,&layout,AV_SAMPLE_FMT_S16,48000,&m.codec->ch_layout,m.codec->sample_fmt,m.codec->sample_rate,0,nullptr),"Configure audio conversion");
        check(swr_init(m.resampler),"Initialize audio conversion");
        duration_=m.format->duration>0?double(m.format->duration)/AV_TIME_BASE:0;
        status("Playing audio only");int64_t samples=0;double base=0;
        auto consume=[&]{
            while(!abort_){
                int rc=avcodec_receive_frame(m.codec,m.frame);if(rc==AVERROR(EAGAIN)||rc==AVERROR_EOF)break;check(rc,"Decode audio");
                int capacity=av_rescale_rnd(swr_get_delay(m.resampler,m.codec->sample_rate)+m.frame->nb_samples,48000,m.codec->sample_rate,AV_ROUND_UP);
                std::vector<uint8_t> pcm(capacity*4);uint8_t* output=pcm.data();
                int count=swr_convert(m.resampler,&output,capacity,const_cast<const uint8_t**>(m.frame->extended_data),m.frame->nb_samples);check(count,"Convert audio");
                while(!abort_ && SDL_GetQueuedAudioSize(device_)>192000*3 && seek_<0) {
                    position_=base+double(samples)/48000-double(SDL_GetQueuedAudioSize(device_))/192000;SDL_Delay(15);
                }
                if(abort_ || seek_>=0){av_frame_unref(m.frame);break;}
                if(SDL_QueueAudio(device_,pcm.data(),count*4)!=0)throw std::runtime_error(SDL_GetError());
                samples+=count;position_=base+double(samples)/48000-double(SDL_GetQueuedAudioSize(device_))/192000;av_frame_unref(m.frame);
            }
        };
        bool eof=false;
        while(!abort_){
            double target=seek_.exchange(-1);
            if(target>=0){
                check(av_seek_frame(m.format,-1,int64_t(target*AV_TIME_BASE),AVSEEK_FLAG_BACKWARD),"Seek audio");
                avcodec_flush_buffers(m.codec);swr_close(m.resampler);check(swr_init(m.resampler),"Reset audio conversion");SDL_ClearQueuedAudio(device_);samples=0;base=target;position_=target;
            }
            int rc=av_read_frame(m.format,m.packet);
            if(rc==AVERROR_EOF){if(!reader.error.empty())throw std::runtime_error(reader.error);eof=true;avcodec_send_packet(m.codec,nullptr);consume();break;}
            if(rc<0){if(!reader.error.empty())throw std::runtime_error(reader.error);check(rc,"Read audio");}
            if(m.packet->stream_index==track){check(avcodec_send_packet(m.codec,m.packet),"Send audio packet");consume();}
            av_packet_unref(m.packet);
        }
        while(eof && !abort_ && SDL_GetQueuedAudioSize(device_)>0){position_=base+double(samples)/48000-double(SDL_GetQueuedAudioSize(device_))/192000;SDL_Delay(20);}
        if(!abort_){active_=false;done_=true;status("Track finished");}
    }catch(const std::exception& e){if(!abort_){status(e.what());active_=false;SDL_ClearQueuedAudio(device_);}}
}
}
