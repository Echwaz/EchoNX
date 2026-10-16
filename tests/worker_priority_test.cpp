#define CURL_GLOBAL_DEFAULT 0
inline int curl_global_init(long){return 0;}
inline void curl_global_cleanup(){}
#define main embedded_main
#include "../src/app.cpp"
#undef main
#include <cassert>
#include <iostream>
static std::atomic<int> homes{0},slowLyrics{0};static std::atomic<bool> homeStarted{false},homeCancelled{false},searched{false},fastLyrics{false};
namespace echo {
std::vector<Song> Api::home(bool,std::atomic<bool>*){return {};}
std::vector<Song> Api::for_saved(const std::vector<Song>&,size_t,bool,std::atomic<bool>* cancel){if(++homes==1){homeStarted=true;while(!cancel->load())SDL_Delay(2);homeCancelled=true;throw std::runtime_error("Cancelled");}return {};}
std::vector<Song> Api::search(const std::string&,bool,std::atomic<bool>*,int){searched=true;return {};}
std::vector<Song> Api::related(const std::string&,bool,std::atomic<bool>*){return {};}
std::vector<Song> Api::artist(const Song&,bool,std::atomic<bool>*){return {};}
std::vector<Song> Api::album(const Song&,bool,std::atomic<bool>*){return {};}
Lyrics Api::lyrics(const Song& song,std::atomic<bool>* cancel){if(song.id=="slow"){++slowLyrics;while(!cancel->load())SDL_Delay(2);throw std::runtime_error("Cancelled");}fastLyrics=true;return {{},{"Ready"},"Ready"};}
std::string http(const std::string&,const std::string&,const std::vector<std::string>&,std::atomic<bool>*,const char*,size_t){throw std::runtime_error("Unexpected network request");}
}
template<class F>void until(F ready){auto start=SDL_GetTicks();while(!ready()&&SDL_GetTicks()-start<2000)SDL_Delay(2);assert(ready());}
int main(){std::filesystem::create_directories("tests");{
    echo::App app;until([]{return homeStarted.load();});app.query_="query";auto start=SDL_GetTicks();app.fetch(1,app.query_);until([]{return searched.load();});assert(homeCancelled&&SDL_GetTicks()-start<750);until([]{return homes.load()>=2;});
    echo::Song slow,fast;slow.id="slow";fast.id="fast";app.requestLyrics(slow);until([]{return slowLyrics.load()==1;});start=SDL_GetTicks();app.requestLyrics(fast);until([]{return fastLyrics.load();});assert(SDL_GetTicks()-start<750);until([&]{app.applyPending();return app.lyricCache_["fast"].plain.size()==1;});
    app.requestLyrics(slow);until([]{return slowLyrics.load()==2;});app.requestLyrics(fast);until([&]{std::lock_guard<std::mutex> lock(app.dataMutex_);return !app.lyricCancel_;});assert(app.lyricCache_["fast"].plain[0]=="Ready");
}std::cout<<"PASS: active Home preempted by Search, interrupted Home resumed, old lyric cancelled, skipped lyric can retry, cached lyric cancels obsolete request.\n";}
