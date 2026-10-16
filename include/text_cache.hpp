#pragma once
#include <SDL2/SDL_ttf.h>
#include <map>
#include <string>
#include <cstdint>
namespace echo {
class TextCache {
public:
    struct Entry{SDL_Texture* texture=nullptr;int width=0,height=0;Uint32 started=0;std::uint64_t used=0,frame=0;size_t bytes=0;};
private:
    std::map<std::string,Entry> entries_;std::uint64_t clock_=0,frame_=0;size_t bytes_=0;
    struct Fit{std::string text;std::uint64_t used;};std::map<std::string,Fit> fits_;
public:
    ~TextCache(){clear();}
    void clear(){for(auto& item:entries_)SDL_DestroyTexture(item.second.texture);entries_.clear();fits_.clear();bytes_=0;}
    void beginFrame(){++frame_;}
    size_t size()const{return entries_.size();}
    std::string fit(TTF_Font* font,const std::string& text,int width){
        std::string key=std::to_string(reinterpret_cast<std::uintptr_t>(font))+"|"+std::to_string(width)+"|"+text;auto found=fits_.find(key);if(found!=fits_.end()){found->second.used=++clock_;return found->second.text;}
        int w=0,h=0;std::string value=text;TTF_SizeUTF8(font,value.c_str(),&w,&h);
        if(w>width)while(value.size()>1){size_t cut=value.size()-1;while(cut>0&&(static_cast<unsigned char>(value[cut])&0xc0)==0x80)--cut;value.resize(cut);auto clipped=value+"...";TTF_SizeUTF8(font,clipped.c_str(),&w,&h);if(w<=width){value=clipped;break;}}
        if(fits_.size()>=256){auto oldest=fits_.begin();for(auto it=fits_.begin();it!=fits_.end();++it)if(it->second.used<oldest->second.used)oldest=it;fits_.erase(oldest);}fits_[key]={value,++clock_};return value;
    }
    const Entry* get(SDL_Renderer* renderer,TTF_Font* font,const std::string& text,int wrap=0){
        if(text.empty())return nullptr;std::string key=std::to_string(reinterpret_cast<std::uintptr_t>(font))+"|"+std::to_string(wrap)+"|"+text;auto found=entries_.find(key);
        if(found==entries_.end()){
            auto* surface=wrap?TTF_RenderUTF8_Blended_Wrapped(font,text.c_str(),{255,255,255,255},wrap):TTF_RenderUTF8_Blended(font,text.c_str(),{255,255,255,255});if(!surface)return nullptr;
            Entry entry;entry.texture=SDL_CreateTextureFromSurface(renderer,surface);entry.width=surface->w;entry.height=surface->h;entry.started=SDL_GetTicks();entry.bytes=size_t(surface->w)*surface->h*4;SDL_FreeSurface(surface);if(!entry.texture)return nullptr;
            found=entries_.emplace(key,entry).first;bytes_+=entry.bytes;
        }
        found->second.used=++clock_;found->second.frame=frame_;
        while(entries_.size()>256||bytes_>16*1024*1024){auto victim=entries_.end();for(auto it=entries_.begin();it!=entries_.end();++it)if(it!=found&&it->second.frame!=frame_&&(victim==entries_.end()||it->second.used<victim->second.used))victim=it;if(victim==entries_.end())break;bytes_-=victim->second.bytes;SDL_DestroyTexture(victim->second.texture);entries_.erase(victim);}
        return &found->second;
    }
    static void tint(const Entry& entry,SDL_Color color){SDL_SetTextureColorMod(entry.texture,color.r,color.g,color.b);SDL_SetTextureAlphaMod(entry.texture,color.a);}
};
}
