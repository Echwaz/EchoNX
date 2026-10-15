#pragma once
#include "visuals.hpp"
namespace echo {
inline SDL_Surface* battery_icon(int percent,bool charging,SDL_Color color){
    return smooth_shape(66,28,color,[&](double x,double y){
        auto inside=[](double x,double y,double left,double top,double right,double bottom,double radius){double dx=x-std::clamp(x,left+radius,right-radius),dy=y-std::clamp(y,top+radius,bottom-radius);return dx*dx+dy*dy<=radius*radius;};
        bool outline=inside(x,y,0,2,42,26,4)&&!inside(x,y,2,4,40,24,2);
        bool fill=percent>0&&x>=5&&x<=5+32*std::clamp(percent,0,100)/100.&&y>=7&&y<=21;
        bool bolt=false;
        if(charging){
            constexpr double vx[]={60,51,57,54,65,59},vy[]={4,16,16,24,12,12};
            for(int i=0,j=5;i<6;j=i++)if((vy[i]>y)!=(vy[j]>y)&&x<(vx[j]-vx[i])*(y-vy[i])/(vy[j]-vy[i])+vx[i])bolt=!bolt;

        }
        return outline||fill||bolt||(x>=44&&x<=48&&y>=10&&y<=18);
    });
}
}
