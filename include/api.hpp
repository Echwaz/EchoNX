#pragma once
#include "model.hpp"
#include "lyrics.hpp"
#include <atomic>
#include <mutex>

namespace echo {
constexpr const char* browserUA="Mozilla/5.0";
constexpr const char* visionUA="com.google.ios.youtube/1.02 (Macintosh; Intel Mac OS X 10_15_4) AppleWebKit/605.1.15 (KHTML, like Gecko)";
struct Stream {std::string url; int64_t length=0;std::string agent;};
std::string http(const std::string& url, const std::string& body="",
    const std::vector<std::string>& headers={}, std::atomic<bool>* abort=nullptr,
    const char* agent=browserUA, size_t maxBytes=8*1024*1024);
class Api {
    std::mutex mutex_;std::string visitor_;
    Json context(); Json call(const std::string& endpoint,Json payload);
public:
    std::vector<Song> home();
    std::vector<Song> search(const std::string& query);
    std::vector<Song> related(const std::string& id);
    std::vector<Song> artist(const Song& song);
    std::vector<Song> album(const Song& song);
    Lyrics lyrics(const Song& song,std::atomic<bool>* abort=nullptr);
    std::vector<Song> for_saved(const std::vector<Song>& saved,size_t offset=0);
    Stream resolve(const std::string& id,std::atomic<bool>* abort);
};
}
