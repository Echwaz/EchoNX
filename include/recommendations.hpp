#pragma once
#include "model.hpp"
#include <algorithm>
#include <unordered_map>

namespace echo {
inline std::vector<Song> recommendation_seeds(const std::vector<Song>& saved,size_t offset=0){
    std::vector<Song> out;std::unordered_set<std::string> artists;
    for(size_t n=0;n<saved.size() && out.size()<3;n++){
        const auto& s=saved[(saved.size()-1-(n+offset)%saved.size())];
        const auto& artist=s.artistId.empty()?s.artist:s.artistId;
        if(artists.insert(artist.empty()?s.id:artist).second)out.push_back(s);
    }
    return out;
}
inline std::vector<Song> rank_recommendations(const std::vector<Song>& saved,const std::vector<std::vector<Song>>& feeds,size_t limit=40){
    std::unordered_set<std::string> excluded,artists;for(const auto& s:saved){excluded.insert(s.id);if(!s.artistId.empty())artists.insert(s.artistId);else if(!s.artist.empty())artists.insert(s.artist);}
    struct Candidate{Song song;double score=0;size_t order=0;};std::vector<Candidate> candidates;std::unordered_map<std::string,size_t> indices;
    for(const auto& feed:feeds){std::unordered_set<std::string> seen;
        for(size_t i=0;i<feed.size();i++){const auto& s=feed[i];if(s.id.empty()||excluded.count(s.id)||!seen.insert(s.id).second)continue;
            if(!indices.count(s.id)){indices[s.id]=candidates.size();candidates.push_back({s,0,candidates.size()});}
            auto& c=candidates[indices[s.id]];c.score+=1.0/(1.0+i/8.0);
            if(artists.count(s.artistId.empty()?s.artist:s.artistId))c.score+=0.25;
        }
    }
    std::stable_sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){return a.score>b.score;});
    std::vector<Song> out;std::unordered_map<std::string,size_t> artistCounts;
    for(const auto& c:candidates){auto artist=c.song.artistId.empty()?c.song.artist:c.song.artistId;if(!artist.empty()&&artistCounts[artist]>=5)continue;out.push_back(c.song);if(!artist.empty())artistCounts[artist]++;if(out.size()==limit)break;}
    return out;
}
}
