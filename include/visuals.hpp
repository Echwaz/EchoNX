#pragma once
#include <array>
#include <vector>
#include <algorithm>
#include <cmath>
#include <string>
#include <cstring>
#include <SDL2/SDL.h>
namespace echo {
struct Triangle {int left,width,height;bool right;};
inline Triangle play_shape(int center){return {center-13,30,17,true};}
template<class Inside> inline SDL_Surface* smooth_shape(int width,int height,SDL_Color color,Inside inside){
    auto* s=SDL_CreateRGBSurfaceWithFormat(0,width,height,32,SDL_PIXELFORMAT_RGBA32);if(!s)return nullptr;
    for(int y=0;y<height;y++)for(int x=0;x<width;x++){int coverage=0;for(int sy=0;sy<4;sy++)for(int sx=0;sx<4;sx++)coverage+=inside(x+(sx+.5)/4,y+(sy+.5)/4)?1:0;Uint32 pixel=SDL_MapRGBA(s->format,color.r,color.g,color.b,Uint8(color.a*coverage/16));std::memcpy(static_cast<Uint8*>(s->pixels)+y*s->pitch+x*4,&pixel,4);}return s;
}
inline Triangle skip_shape(int center,bool next){return {center+(next?-14:-12),26,13,next};}
inline std::string lyric_display(const std::string& s){return s.find_first_not_of(" \t\r\n")==std::string::npos?"...":s;}
inline double lyric_ease(double t){t=std::clamp(t,0.0,1.0);return 1-std::pow(1-t,3);}
inline double title_scroll(double overflow,double seconds){if(overflow<=0)return 0;double travel=overflow/24.0,t=std::fmod(std::max(0.0,seconds),travel+3);if(t<1.5)return 0;return std::min(overflow,(t-1.5)*24);}
inline float analog_scroll_speed(int left,int right){int value=std::abs(left)>=std::abs(right)?left:right;float normalized=std::clamp(value/32767.f,-1.f,1.f);constexpr float dead=.22f;if(std::abs(normalized)<=dead)return 0;return -std::copysign((std::abs(normalized)-dead)/(1-dead)*480,normalized);}
inline std::string audio_target_label(int target){switch(target){case 1:return "Speaker";case 2:return "Headset";case 3:return "TV audio";case 4:return "USB audio";case 5:return "Bluetooth";default:return "Audio: --";}}
inline SDL_Texture* album_blur(SDL_Renderer* renderer,SDL_Surface* source,float brightness=.19f){
    constexpr int width=64,height=36;
    auto* small=SDL_CreateRGBSurfaceWithFormat(0,width,height,32,SDL_PIXELFORMAT_RGBA32);if(!small)return nullptr;
    SDL_BlitScaled(source,nullptr,small,nullptr);
    std::vector<std::array<float,3>> pixels(width*height),next(pixels.size());
    for(int y=0;y<height;y++)for(int x=0;x<width;x++){Uint8 r,g,b,a;Uint32 value;std::memcpy(&value,static_cast<Uint8*>(small->pixels)+y*small->pitch+x*4,4);SDL_GetRGBA(value,small->format,&r,&g,&b,&a);pixels[y*width+x]={float(r),float(g),float(b)};}
    for(int pass=0;pass<4;pass++)for(int axis=0;axis<2;axis++){
        for(int y=0;y<height;y++)for(int x=0;x<width;x++){std::array<float,3> sum{};for(int d=-5;d<=5;d++){int xx=axis==0?std::clamp(x+d,0,width-1):x,yy=axis==1?std::clamp(y+d,0,height-1):y;for(int c=0;c<3;c++)sum[c]+=pixels[yy*width+xx][c];}for(int c=0;c<3;c++)next[y*width+x][c]=sum[c]/11;}pixels.swap(next);
    }
    for(int y=0;y<height;y++)for(int x=0;x<width;x++){const auto& p=pixels[y*width+x];Uint32 value=SDL_MapRGBA(small->format,Uint8(p[0]*brightness),Uint8(p[1]*brightness),Uint8(p[2]*brightness),255);std::memcpy(static_cast<Uint8*>(small->pixels)+y*small->pitch+x*4,&value,4);}
    auto* texture=SDL_CreateTextureFromSurface(renderer,small);SDL_FreeSurface(small);return texture;
}
}
