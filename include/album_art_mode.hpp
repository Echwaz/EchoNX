#pragma once
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace echo {
inline SDL_Color album_art_color(SDL_Surface* source) {
    if(!source)return {32,34,39,255};
    auto* sample=SDL_ConvertSurfaceFormat(source,SDL_PIXELFORMAT_RGBA32,0);
    if(!sample)return {32,34,39,255};
    struct Bin{double weight=0,r=0,g=0,b=0;};Bin bins[512]{};
    int stride=std::max(1,std::max(sample->w,sample->h)/48);
    for(int y=0;y<sample->h;y+=stride)for(int x=0;x<sample->w;x+=stride){
        Uint8 r,g,b,a;auto pixel=reinterpret_cast<Uint32*>(static_cast<Uint8*>(sample->pixels)+y*sample->pitch)[x];
        SDL_GetRGBA(pixel,sample->format,&r,&g,&b,&a);
        int high=std::max({r,g,b}),low=std::min({r,g,b});
        if(a<128||high<24||low>232)continue;
        double weight=.25+double(high-low)/255.;
        auto& bin=bins[(r/32)*64+(g/32)*8+b/32];
        bin.weight+=weight;bin.r+=r*weight;bin.g+=g*weight;bin.b+=b*weight;
    }
    SDL_FreeSurface(sample);
    const auto* best=&bins[0];for(const auto& bin:bins)if(bin.weight>best->weight)best=&bin;
    if(best->weight==0)return {40,42,46,255};
    double r=best->r/best->weight,g=best->g/best->weight,b=best->b/best->weight;
    double scale=std::clamp(112./std::max({r,g,b}),.42,1.3);
    return {Uint8(std::clamp(r*scale,0.,255.)),Uint8(std::clamp(g*scale,0.,255.)),Uint8(std::clamp(b*scale,0.,255.)),255};
}
inline SDL_Surface* album_art_shadow() {
    auto* s=SDL_CreateRGBSurfaceWithFormat(0,608,608,32,SDL_PIXELFORMAT_RGBA32);
    if(!s)return nullptr;
    for(int y=0;y<s->h;++y)for(int x=0;x<s->w;++x){
        double dx=std::max(std::abs(x+.5-304)-256.,0.),dy=std::max(std::abs(y+.5-304)-256.,0.);
        double distance=std::max(0.,std::sqrt(dx*dx+dy*dy)-24.);
        auto alpha=Uint8(std::lround(75*std::exp(-distance*distance/200.)));
        reinterpret_cast<Uint32*>(static_cast<Uint8*>(s->pixels)+y*s->pitch)[x]=SDL_MapRGBA(s->format,0,0,0,alpha);
    }
    return s;
}
inline void rounded_album_art(SDL_Renderer* renderer,SDL_Texture* texture,SDL_FRect r,float radius=24) {
    if(!texture)return;
    std::vector<SDL_Vertex> vertices{{{r.x+r.w/2,r.y+r.h/2},{255,255,255,255},{.5f,.5f}}};
    std::vector<int> indices;
    for(int corner=0;corner<4;++corner){
        float cx=r.x+(corner<2?r.w-radius:radius);
        float cy=r.y+(corner==0||corner==3?radius:r.h-radius);
        for(int step=0;step<=16;++step){
            double angle=(-90+corner*90+step*90./16)*3.141592653589793/180;
            for(int edge=0;edge<2;++edge){
                float rad=radius+(edge?.5f:-.5f),x=cx+rad*std::cos(angle),y=cy+rad*std::sin(angle);
                vertices.push_back({{x,y},{255,255,255,Uint8(edge?0:255)},{std::clamp((x-r.x)/r.w,0.f,1.f),std::clamp((y-r.y)/r.h,0.f,1.f)}});
            }
        }
    }
    int count=(int(vertices.size())-1)/2;
    for(int i=0;i<count;++i){int a=1+i*2,b=1+((i+1)%count)*2;indices.insert(indices.end(),{0,a,b,a,a+1,b,b,a+1,b+1});}
    SDL_BlendMode old;SDL_GetTextureBlendMode(texture,&old);SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(renderer,texture,vertices.data(),int(vertices.size()),indices.data(),int(indices.size()));
    SDL_SetTextureBlendMode(texture,old);
}
}
