#pragma once
#include "local_state.hpp"
#include <set>
namespace echo {
constexpr size_t history_limit=200;
struct HistoryEntry{Song song;std::int64_t playedAt=0;};
inline void record_listen(std::vector<HistoryEntry>& history,const Song& song,std::int64_t playedAt){
    if(song.id.empty())return;
    history.erase(std::remove_if(history.begin(),history.end(),[&](const HistoryEntry& item){return item.song.id==song.id;}),history.end());
    history.insert(history.begin(),{song,std::max<std::int64_t>(0,playedAt)});if(history.size()>history_limit)history.resize(history_limit);
}
inline Json history_document(const std::vector<HistoryEntry>& history){Json rows=Json::array();for(const auto& item:history)rows.push_back({{"song",to_json(item.song)},{"playedAt",item.playedAt}});return {{"version",1},{"history",rows}};}
inline std::vector<HistoryEntry> load_history(const std::string& path){
    for(const auto& candidate:{path,path+".bak"})try{
        auto root=read_document(candidate);if(!root.is_object()||root.value("version",0)!=1||!root.at("history").is_array())continue;
        std::vector<HistoryEntry> result;std::set<std::string> seen;
        for(const auto& row:root.at("history")){auto song=from_json(row.at("song"));auto time=row.at("playedAt").get<std::int64_t>();if(song.id.empty()||time<0)throw std::runtime_error("Invalid history");if(seen.insert(song.id).second&&result.size()<history_limit)result.push_back({song,time});}
        return result;
    }catch(...){ }
    return {};
}
}
