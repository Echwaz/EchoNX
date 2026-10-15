#pragma once
#include "disk_cache.hpp"
#include "model.hpp"
namespace echo {
inline std::optional<std::string> cached_artwork(DiskCache& cache,const Song& song){
    if(!song.cover.empty())if(auto bytes=cache.get("get-v1|"+song.cover,30*86400))return bytes;
    if(auto previous=cache.get("art-ref|"+song.id,30*86400))return cache.get("get-v1|"+*previous,30*86400);
    return {};
}
}