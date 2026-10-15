#pragma once
#include "model.hpp"
#include "playback_order.hpp"
namespace echo {
inline bool enqueue_next(std::vector<Song>& queue,int& current,const Song& song,PlaybackOrder& order){
    if(song.id.empty()||current<0||current>=int(queue.size())||queue[current].id==song.id)return false;
    Song selected=song;
    for(int i=0;i<int(queue.size());){if(queue[i].id==selected.id){queue.erase(queue.begin()+i);if(i<current)--current;}else ++i;}
    queue.insert(queue.begin()+current+1,selected);
    order.reset(int(queue.size()),current);order.promoteNext(current+1);return true;
}
}