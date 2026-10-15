#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
namespace echo {
class SeekBarState {
    int x_=0,y_=0,width_=1;double duration_=0,target_=0,visual_=0,emphasis_=0;std::string track_;
    bool dragging_=false,pending_=false;std::uint32_t last_=0,released_=0;
public:
    void configure(int x,int y,int width,const std::string& track,double duration,double position,std::uint32_t now){
        x_=x;y_=y;width_=std::max(1,width);duration_=duration;
        if(track_!=track){track_=track;dragging_=pending_=false;visual_=duration>0?std::clamp(position/duration,0.0,1.0):0;target_=visual_;emphasis_=0;last_=now;}
    }
    bool begin(int x,int y){if(duration_<=0||x<x_-12||x>x_+width_+12||y<y_-20||y>y_+32)return false;dragging_=true;pending_=false;move(x);return true;}
    void move(int x){if(dragging_)target_=std::clamp((x-x_)/double(width_),0.0,1.0);}
    std::optional<double> release(const std::string& track,std::uint32_t now){if(!dragging_)return {};dragging_=false;if(track!=track_||duration_<=0)return {};pending_=true;released_=now;return std::min(target_*duration_,std::max(0.0,duration_-.1));}
    void cancel(){dragging_=pending_=false;}
    double advance(double position,std::uint32_t now){
        if(pending_&&(std::abs(position-target_*duration_)<.75||now-released_>5000))pending_=false;
        double desired=(dragging_||pending_)?target_:(duration_>0?std::clamp(position/duration_,0.0,1.0):0);
        double factor=1-std::exp(-double(std::min<std::uint32_t>(now-last_,100))/55.0);last_=now;visual_+=(desired-visual_)*factor;emphasis_+=((dragging_?1.0:0.0)-emphasis_)*factor;return visual_;
    }
    bool dragging()const{return dragging_;}
    double emphasis()const{return emphasis_;}
    double displayTime(double position)const{return dragging_||pending_?target_*duration_:position;}
};
}