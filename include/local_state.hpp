#pragma once
#include "library_store.hpp"
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

namespace echo {
inline void save_document(const std::string& path,const Json& document,RenameFile rename=[](const std::string& from,const std::string& to){return std::rename(from.c_str(),to.c_str());}){
    auto parent=std::filesystem::path(path).parent_path();if(!parent.empty())std::filesystem::create_directories(parent);
    std::ofstream output(path+".tmp",std::ios::trunc);output<<document.dump();output.close();if(!output)throw std::runtime_error("Cannot write local data on SD.");
    bool original=std::filesystem::exists(path);
    if(original){std::error_code error;std::filesystem::remove(path+".bak",error);if(error||rename(path,path+".bak")!=0)throw std::runtime_error("Cannot back up local data on SD.");}
    if(rename(path+".tmp",path)!=0){if(original)rename(path+".bak",path);throw std::runtime_error("Cannot replace local data on SD. Previous data preserved.");}
}
inline Json read_document(const std::string& path){
    std::error_code error;auto size=std::filesystem::file_size(path,error);if(error||size>16*1024*1024)return {};
    std::ifstream input(path);if(!input)return {};return Json::parse(input,nullptr,false);
}
struct SessionSnapshot {std::vector<Song> queue;std::vector<int> order;int current=-1,repeat=0;bool shuffle=false;double position=0,duration=0;};
inline Json session_document(const SessionSnapshot& s){Json rows=Json::array();for(const auto& song:s.queue)rows.push_back(to_json(song));return {{"version",1},{"queue",rows},{"order",s.order},{"current",s.current},{"repeat",s.repeat},{"shuffle",s.shuffle},{"position",s.position},{"duration",s.duration}};}
inline std::optional<SessionSnapshot> parse_session(const Json& root){
    try{
        if(!root.is_object()||root.value("version",0)!=1||!root.at("queue").is_array())return {};
        SessionSnapshot s;for(const auto& row:root.at("queue")){auto song=from_json(row);if(song.id.empty())return {};s.queue.push_back(song);}
        s.order=root.at("order").get<std::vector<int>>();s.current=root.at("current").get<int>();s.repeat=root.at("repeat").get<int>();s.shuffle=root.at("shuffle").get<bool>();s.position=root.at("position").get<double>();s.duration=root.value("duration",0.);
        if(s.queue.size()!=s.order.size()||s.repeat<0||s.repeat>2||!std::isfinite(s.position)||s.position<0||!std::isfinite(s.duration)||s.duration<0)return {};
        if(s.queue.empty()){if(s.current!=-1)return {};}else if(s.current<0||s.current>=int(s.queue.size()))return {};
        auto sorted=s.order;std::sort(sorted.begin(),sorted.end());for(int i=0;i<int(sorted.size());++i)if(sorted[i]!=i)return {};
        if(s.duration>0)s.position=std::min(s.position,std::max(0.,s.duration-.1));return s;
    }catch(...){return {};}
}
inline SessionSnapshot load_session(const std::string& path){for(const auto& candidate:{path,path+".bak"})if(auto session=parse_session(read_document(candidate)))return *session;return {};}
struct LocalPlaylist {std::string id,name;std::vector<Song> tracks;};
inline Json playlists_document(const std::vector<LocalPlaylist>& lists){Json rows=Json::array();for(const auto& list:lists){Json songs=Json::array();for(const auto& song:list.tracks)songs.push_back(to_json(song));rows.push_back({{"id",list.id},{"name",list.name},{"tracks",songs}});}return {{"version",1},{"playlists",rows}};}
inline std::vector<LocalPlaylist> load_playlists(const std::string& path){
    for(const auto& candidate:{path,path+".bak"})try{
        auto root=read_document(candidate);if(!root.is_object()||root.value("version",0)!=1||!root.at("playlists").is_array())continue;
        std::vector<LocalPlaylist> lists;std::vector<std::string> ids;
        for(const auto& row:root.at("playlists")){LocalPlaylist list;list.id=row.at("id").get<std::string>();list.name=row.at("name").get<std::string>();if(list.id.empty()||list.name.empty()||!row.at("tracks").is_array()||std::find(ids.begin(),ids.end(),list.id)!=ids.end())throw std::runtime_error("Invalid playlist");ids.push_back(list.id);for(const auto& track:row.at("tracks")){auto song=from_json(track);if(song.id.empty())throw std::runtime_error("Invalid track");list.tracks.push_back(song);}lists.push_back(std::move(list));}return lists;
    }catch(...){ }
    return {};
}
inline bool playlist_add(LocalPlaylist& list,const Song& song){if(song.id.empty()||std::any_of(list.tracks.begin(),list.tracks.end(),[&](const Song& track){return track.id==song.id;}))return false;list.tracks.push_back(song);return true;}
class SessionWriter {
    std::string path_,error_;std::mutex mutex_;std::condition_variable cv_;struct Pending{Json document;std::uint64_t ticket;};std::optional<Pending> pending_;std::uint64_t submitted_=0,completed_=0;bool closing_=false;std::thread thread_;
public:
    explicit SessionWriter(std::string path):path_(std::move(path)),thread_([this]{
        Json lastSaved;for(;;){Json document;std::uint64_t ticket;{std::unique_lock<std::mutex> lock(mutex_);cv_.wait(lock,[&]{return closing_||pending_.has_value();});if(!pending_&&closing_)break;document=std::move(pending_->document);ticket=pending_->ticket;pending_.reset();}
            try{if(document!=lastSaved){save_document(path_,document);lastSaved=std::move(document);}std::lock_guard<std::mutex> lock(mutex_);completed_=ticket;}catch(const std::exception& e){std::lock_guard<std::mutex> lock(mutex_);error_=e.what();}
        }
    }){}
    ~SessionWriter(){{std::lock_guard<std::mutex> lock(mutex_);closing_=true;}cv_.notify_one();if(thread_.joinable())thread_.join();}
    std::uint64_t submit(Json snapshot){std::uint64_t ticket;{std::lock_guard<std::mutex> lock(mutex_);ticket=++submitted_;pending_=Pending{std::move(snapshot),ticket};}cv_.notify_one();return ticket;}
    std::uint64_t completed(){std::lock_guard<std::mutex> lock(mutex_);return completed_;}
    std::string takeError(){std::lock_guard<std::mutex> lock(mutex_);auto error=std::move(error_);error_.clear();return error;}
};
}
