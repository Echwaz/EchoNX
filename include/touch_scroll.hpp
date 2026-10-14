#pragma once
#include <algorithm>
#include <cmath>
namespace echo {
struct TouchScroll {
    float offset=0,startY=0,lastY=0;bool active=false,dragged=false;
    void begin(float y){active=true;dragged=false;startY=lastY=y;}
    void move(float y,float maximum){if(!active)return;if(std::fabs(y-startY)>8)dragged=true;if(dragged)offset=std::clamp(offset+lastY-y,0.f,std::max(0.f,maximum));lastY=y;}
    bool end(){bool tap=active&&!dragged;active=false;return tap;}
    void reset(){offset=0;active=false;dragged=false;}
};
}
