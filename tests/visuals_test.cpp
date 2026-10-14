#include "visuals.hpp"
#include <cassert>
#include <iostream>
int main(){
    auto play=echo::play_shape(100);assert(play.left+play.width/3==97);
    auto next=echo::skip_shape(100,true),prev=echo::skip_shape(100,false);
    assert(next.left==200-prev.left-prev.width);assert(next.width==prev.width);
    assert(echo::lyric_display(" \t\n")=="...");assert(echo::lyric_display("Line A")=="Line A");
    assert(echo::lyric_ease(0)==0&&echo::lyric_ease(1)==1);assert(echo::lyric_ease(.5)>.5);
    assert(echo::title_scroll(0,10)==0);assert(echo::title_scroll(240,5)==120);assert(echo::title_scroll(240,12)==288);assert(echo::title_scroll(240,13)==8);double period=304.0/24;double before=echo::title_scroll(240,period-.001),after=echo::title_scroll(240,period+.001);assert(std::abs((after+304)-before-.048)<.0001);
    assert(echo::analog_scroll_speed(3000,-3000)==0);assert(echo::analog_scroll_speed(-32767,0)>479);assert(echo::analog_scroll_speed(0,32767)<-479);assert(echo::analog_scroll_speed(0,-32767)==echo::analog_scroll_speed(-32767,0));
    assert(echo::audio_target_label(1)=="Speaker"&&echo::audio_target_label(2)=="Headset"&&echo::audio_target_label(5)=="Bluetooth"&&echo::audio_target_label(0)=="Audio: --");
    SDL_Init(SDL_INIT_VIDEO);auto* window=SDL_CreateWindow("test",0,0,64,36,0);auto* renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
    auto* edge=echo::smooth_shape(8,8,{255,255,255,255},[](double x,double y){return x<y*.7;});bool partial=false;for(int y=0;y<8;y++)for(int x=0;x<8;x++){Uint32 pixel;std::memcpy(&pixel,static_cast<Uint8*>(edge->pixels)+y*edge->pitch+x*4,4);Uint8 r,g,b,a;SDL_GetRGBA(pixel,edge->format,&r,&g,&b,&a);partial|=a>0&&a<255;}assert(partial);SDL_FreeSurface(edge);
    auto* source=SDL_CreateRGBSurfaceWithFormat(0,64,36,32,SDL_PIXELFORMAT_RGBA32);SDL_FillRect(source,nullptr,SDL_MapRGBA(source->format,255,100,50,255));
    auto* blur=echo::album_blur(renderer,source);assert(blur);SDL_RenderCopy(renderer,blur,nullptr,nullptr);Uint32 pixel=0;SDL_Rect point{30,18,1,1};SDL_RenderReadPixels(renderer,&point,SDL_PIXELFORMAT_RGBA32,&pixel,4);Uint8 r,g,b,a;SDL_GetRGBA(pixel,source->format,&r,&g,&b,&a);assert(r==48&&g==19&&b==9);
    SDL_DestroyTexture(blur);SDL_FreeSurface(source);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();std::cout<<"PASS: icon centering and symmetry, blank lyrics, transition easing, dark album-derived background.\n";
}
