#pragma once
#include "visuals.hpp"
namespace echo {
struct IconPoint {double x,y;};
inline SDL_Surface* playback_icon(bool shuffle,SDL_Color color){
    std::vector<std::pair<IconPoint,IconPoint>> edges;
    auto line=[&](IconPoint a,IconPoint b){edges.push_back({a,b});};
    auto curve=[&](IconPoint a,IconPoint b,IconPoint c,IconPoint d){IconPoint last=a;for(int i=1;i<=24;i++){double t=i/24.0,u=1-t;IconPoint p{u*u*u*a.x+3*u*u*t*b.x+3*u*t*t*c.x+t*t*t*d.x,u*u*u*a.y+3*u*u*t*b.y+3*u*t*t*c.y+t*t*t*d.y};line(last,p);last=p;}};
    if(shuffle){
        line({3,7},{8,7});curve({8,7},{16,7},{16,25},{24,25});line({24,25},{29,25});
        line({3,25},{8,25});curve({8,25},{16,25},{16,7},{24,7});line({24,7},{29,7});
        line({24,2},{29,7});line({29,7},{24,12});line({24,20},{29,25});line({29,25},{24,30});
    }else{
        line({4,16},{4,13});curve({4,13},{4,8},{7,6},{12,6});line({12,6},{28,6});line({23,1},{28,6});line({28,6},{23,11});
        line({28,16},{28,19});curve({28,19},{28,24},{25,26},{20,26});line({20,26},{4,26});line({9,21},{4,26});line({4,26},{9,31});
    }
    return smooth_shape(36,36,color,[&](double x,double y){x-=2;y-=2;for(const auto& edge:edges){auto a=edge.first,b=edge.second;double dx=b.x-a.x,dy=b.y-a.y,t=std::clamp(((x-a.x)*dx+(y-a.y)*dy)/(dx*dx+dy*dy),0.0,1.0);double px=x-a.x-t*dx,py=y-a.y-t*dy;if(px*px+py*py<=1.3*1.3)return true;}return false;});
}
}