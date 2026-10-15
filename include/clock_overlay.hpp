#pragma once
#include "album_art_mode.hpp"
#include <array>
namespace echo {
inline SDL_Surface* clock_backdrop(SDL_Surface* cover){
    auto* canvas=SDL_CreateRGBSurfaceWithFormat(0,128,72,32,SDL_PIXELFORMAT_RGBA32);if(!canvas)return nullptr;
    auto bg=album_art_color(cover);SDL_FillRect(canvas,nullptr,SDL_MapRGBA(canvas->format,bg.r,bg.g,bg.b,255));
    SDL_Rect art{36,8,56,56};if(cover)SDL_BlitScaled(cover,nullptr,canvas,&art);
    std::vector<std::array<float,3>> pixels(128*72),next(pixels.size());
    for(int y=0;y<72;y++)for(int x=0;x<128;x++){Uint8 r,g,b,a;SDL_GetRGBA(reinterpret_cast<Uint32*>(static_cast<Uint8*>(canvas->pixels)+y*canvas->pitch)[x],canvas->format,&r,&g,&b,&a);pixels[y*128+x]={float(r),float(g),float(b)};}
    for(int pass=0;pass<2;pass++)for(int axis=0;axis<2;axis++){
        for(int y=0;y<72;y++)for(int x=0;x<128;x++){std::array<float,3> sum{};for(int d=-2;d<=2;d++){int xx=axis==0?std::clamp(x+d,0,127):x,yy=axis==1?std::clamp(y+d,0,71):y;for(int c=0;c<3;c++)sum[c]+=pixels[yy*128+xx][c];}for(int c=0;c<3;c++)next[y*128+x][c]=sum[c]/5;}pixels.swap(next);
    }
    for(int y=0;y<72;y++)for(int x=0;x<128;x++){auto p=pixels[y*128+x];reinterpret_cast<Uint32*>(static_cast<Uint8*>(canvas->pixels)+y*canvas->pitch)[x]=SDL_MapRGBA(canvas->format,Uint8(p[0]*.8f),Uint8(p[1]*.8f),Uint8(p[2]*.8f),255);}
    return canvas;
}
}
