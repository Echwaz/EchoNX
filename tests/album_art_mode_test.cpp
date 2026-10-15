#include "album_art_mode.hpp"
#include <cassert>
#include <iostream>
int main(){
    SDL_Init(SDL_INIT_VIDEO);
    auto* output=SDL_CreateRGBSurfaceWithFormat(0,640,640,32,SDL_PIXELFORMAT_RGBA32);
    auto* renderer=SDL_CreateSoftwareRenderer(output);
    auto* source=SDL_CreateRGBSurfaceWithFormat(0,8,8,32,SDL_PIXELFORMAT_RGBA32);
    SDL_FillRect(source,nullptr,SDL_MapRGBA(source->format,220,100,80,255));
    auto solid=echo::album_art_color(source);assert(solid.r>solid.g&&solid.g>solid.b&&solid.a==255);
    SDL_FillRect(source,nullptr,SDL_MapRGBA(source->format,255,255,255,255));auto white=echo::album_art_color(source);assert(white.r==40);
    SDL_FillRect(source,nullptr,SDL_MapRGBA(source->format,0,0,0,255));auto black=echo::album_art_color(source);assert(black.r==40);
    SDL_FillRect(source,nullptr,SDL_MapRGBA(source->format,220,100,80,255));
    auto* art=SDL_CreateTextureFromSurface(renderer,source);
    SDL_SetRenderDrawColor(renderer,10,20,30,255);SDL_RenderClear(renderer);
    echo::rounded_album_art(renderer,art,{40,40,560,560});
    auto pixel=[&](int x,int y){Uint8 r,g,b,a;Uint32 p=reinterpret_cast<Uint32*>(static_cast<Uint8*>(output->pixels)+y*output->pitch)[x];SDL_GetRGBA(p,output->format,&r,&g,&b,&a);return int(r);};
    assert(pixel(320,320)==220);
    for(int x:{40,599})for(int y:{40,599})assert(pixel(x,y)==10);
    for(auto point:std::vector<SDL_Point>{{320,41},{598,320},{320,598},{41,320}})assert(pixel(point.x,point.y)==220);
    auto* shadow=echo::album_art_shadow();assert(shadow);
    auto alpha=[&](int x,int y){Uint8 r,g,b,a;Uint32 p=reinterpret_cast<Uint32*>(static_cast<Uint8*>(shadow->pixels)+y*shadow->pitch)[x];SDL_GetRGBA(p,shadow->format,&r,&g,&b,&a);return int(a);};
    assert(alpha(304,304)==75);assert(alpha(304,15)>alpha(304,0));assert(alpha(0,0)==0);

    SDL_FreeSurface(shadow);SDL_DestroyTexture(art);SDL_FreeSurface(source);SDL_DestroyRenderer(renderer);SDL_FreeSurface(output);SDL_Quit();
    std::cout<<"PASS: four rounded corners, intact cover edges and center, soft shadow falloff\n";
}
