#include "listening_history.hpp"
#include <cassert>
#include <iostream>
int main(){
    std::vector<echo::HistoryEntry> rows;echo::Song a{"A","Alpha","Artist"},b{"B","Beta","Artist"};
    echo::record_listen(rows,a,10);echo::record_listen(rows,b,20);assert(rows[0].song.id=="B"&&rows[1].song.id=="A");a.title="Updated title";echo::record_listen(rows,a,30);assert(rows.size()==2&&rows[0].song.title=="Updated title"&&rows[0].playedAt==30);echo::record_listen(rows,{},40);assert(rows.size()==2);
    auto folder=std::filesystem::temp_directory_path()/"echonx-history-test";std::filesystem::create_directories(folder);auto path=(folder/"history.json").string();
    echo::save_document(path,echo::history_document(rows));auto restored=echo::load_history(path);assert(restored.size()==2&&restored[0].song.id=="A"&&restored[0].playedAt==30);
    for(int i=0;i<210;i++){auto song=a;song.id=std::to_string(i);echo::record_listen(rows,song,50+i);}assert(rows.size()==200&&rows.front().song.id=="209"&&rows.back().song.id=="10");
    echo::save_document(path,echo::history_document(rows));{std::ofstream broken(path);broken<<"{bad";}restored=echo::load_history(path);assert(restored.size()==2&&restored.front().song.id=="A");
    echo::save_document(path,echo::history_document({}));assert(echo::load_history(path).empty());
    for(auto& item:std::filesystem::directory_iterator(folder))if(item.is_regular_file())std::filesystem::remove(item.path());std::filesystem::remove(folder);
    std::cout<<"PASS: recent-first ordering, replay deduplication, metadata refresh, 200-song bound, persisted timestamps, backup recovery, empty history\n";
}
