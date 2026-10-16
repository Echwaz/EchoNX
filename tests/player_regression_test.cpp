#include "player.hpp"
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
static std::string audio;
static std::atomic<bool> resolving{false};
namespace echo {
Stream Api::resolve(const std::string& id,std::atomic<bool>* abort){
    if(id=="slow"){resolving=true;for(int n=0;n<300;++n){if(abort->load())throw std::runtime_error("Cancelled");SDL_Delay(10);}}
    if(abort->load())throw std::runtime_error("Cancelled");return {"fixture?audio",int64_t(audio.size()),"test"};
}
std::string http(const std::string& url,const std::string&,const std::vector<std::string>&,std::atomic<bool>* abort,const char*,size_t){
    if(abort&&abort->load())throw std::runtime_error("Cancelled");auto range=url.substr(url.find("&range=")+7);auto start=std::stoull(range),end=std::stoull(range.substr(range.find('-')+1));return audio.substr(start,end-start+1);
}
}
template<class F> static void until(F predicate,int timeout=2000){auto start=SDL_GetTicks();bool ready=predicate();while(!ready&&SDL_GetTicks()-start<Uint32(timeout)){SDL_Delay(5);ready=predicate();}assert(ready);}
int main(int argc,char** argv){
    assert(argc==2);std::ifstream input(argv[1],std::ios::binary);audio={std::istreambuf_iterator<char>(input),{}};assert(!audio.empty());assert(SDL_Init(SDL_INIT_AUDIO)==0);
    {echo::Api api;echo::Player player(api);echo::Song song;song.id="slow";player.play(song);until([]{return resolving.load();});
    auto start=SDL_GetTicks();song.id="fast";player.play(song);assert(SDL_GetTicks()-start<100);until([&]{return player.hasStarted();});
    SDL_Delay(550);player.toggle();double before=player.position();player.seek(.1);until([&]{return player.position()<.4;});assert(player.active()&&player.paused()&&!player.ended());SDL_Delay(100);assert(player.position()<.4&&before>.4);player.toggle();until([&]{return player.ended();},5000);assert(!player.active());
    song.id="slow";resolving=false;player.play(song);until([]{return resolving.load();});start=SDL_GetTicks();player.stop();assert(SDL_GetTicks()-start<100&&!player.active());}
    SDL_Quit();std::cout<<"PASS: nonblocking track replacement/stop, cancellation during resolve, seek during EOF drain while paused, resume and natural completion.\n";
}
