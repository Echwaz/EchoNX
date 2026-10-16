#include "local_state.hpp"
#include "playback_order.hpp"
#include <cassert>
#include <iostream>
int main(){
    namespace fs=std::filesystem;auto folder=fs::temp_directory_path()/"echonx-local-state-test";fs::create_directories(folder);
    auto sessionPath=(folder/"session.json").string(),playlistPath=(folder/"playlists.json").string();
    echo::SessionSnapshot s;s.queue={{"A","Alpha","Artist"},{"B","Beta","Artist"},{"C","Gamma","Artist"}};s.order={2,0,1};s.current=0;s.shuffle=true;s.repeat=2;s.position=87.25;s.duration=240;
    auto decoded=echo::parse_session(echo::session_document(s));assert(decoded&&decoded->order==s.order&&decoded->position==87.25);
    auto bad=echo::session_document(s);bad["order"]={0,0,2};assert(!echo::parse_session(bad));bad=echo::session_document(s);bad["current"]=10;assert(!echo::parse_session(bad));
    {echo::SessionWriter writer(sessionPath);writer.submit(echo::session_document(s));}auto restored=echo::load_session(sessionPath);assert(restored.queue.size()==3&&restored.current==0&&restored.repeat==2&&restored.shuffle);
    {echo::SessionWriter writer(playlistPath);auto ticket=writer.submit({{"value",1}});for(int n=0;n<500&&writer.completed()<ticket;++n)std::this_thread::sleep_for(std::chrono::milliseconds(2));assert(writer.completed()==ticket);ticket=writer.submit({{"value",1}});for(int n=0;n<500&&writer.completed()<ticket;++n)std::this_thread::sleep_for(std::chrono::milliseconds(2));assert(writer.completed()==ticket);}
    auto blocked=folder/"blocked";std::ofstream(blocked)<<"file";{echo::SessionWriter writer((blocked/"data.json").string());writer.submit({{"value",1}});std::string error;for(int n=0;n<500&&error.empty();++n){error=writer.takeError();std::this_thread::sleep_for(std::chrono::milliseconds(2));}assert(!error.empty()&&writer.completed()==0);}fs::remove(blocked);
    echo::PlaybackOrder order;assert(order.restore(restored.order,restored.current));order.repeat=0;assert(order.step(1)==1);assert(!order.restore({0,0},0));
    echo::LocalPlaylist list{"p1","Favorites",{}};assert(echo::playlist_add(list,s.queue[0]));assert(!echo::playlist_add(list,s.queue[0]));assert(echo::playlist_add(list,s.queue[1]));
    echo::save_document(playlistPath,echo::playlists_document({list}));list.name="Renamed";echo::save_document(playlistPath,echo::playlists_document({list}));auto lists=echo::load_playlists(playlistPath);assert(lists.size()==1&&lists[0].name=="Renamed"&&lists[0].tracks.size()==2);
    {std::ofstream broken(playlistPath);broken<<"{";}assert(echo::load_playlists(playlistPath)[0].name=="Favorites");
    echo::save_document(sessionPath,echo::session_document(s));auto before=echo::load_session(sessionPath);try{echo::save_document(sessionPath,echo::session_document(s),[](const std::string& a,const std::string& b){if(a.find(".tmp")!=std::string::npos)return -1;return std::rename(a.c_str(),b.c_str());});assert(false);}catch(const std::exception&){}assert(echo::load_session(sessionPath).order==before.order);
    echo::save_document(playlistPath,echo::playlists_document({}));assert(echo::load_playlists(playlistPath).empty());
    for(const auto& entry:fs::directory_iterator(folder))if(entry.is_regular_file())fs::remove(entry.path());fs::remove(folder);
    std::cout<<"PASS: session and shuffle order roundtrip, writer flush, invalid data rejection, playlist dedup/rename/delete, corrupt-file recovery, failed replacement rollback\n";
}
