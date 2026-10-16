#pragma once
#include "model.hpp"
#include "playback_order.hpp"
namespace echo {
inline bool enqueue_next(std::vector<Song>& queue,int& current,const Song& song,PlaybackOrder& order){
    if(song.id.empty()||current<0||current>=int(queue.size())||queue[current].id==song.id)return false;
    if(order.size()!=int(queue.size()))order.reset(int(queue.size()),current);
    auto previous=order.displayOrder();std::vector<int> mapping(queue.size(),-1);std::vector<Song> updated;
    for(int i=0;i<int(queue.size());++i)if(queue[i].id!=song.id){mapping[i]=int(updated.size());updated.push_back(queue[i]);}
    int nextCurrent=mapping[current],insert=nextCurrent+1;
    for(auto& index:mapping)if(index>=insert)++index;
    updated.insert(updated.begin()+insert,song);std::vector<int> nextOrder;
    for(int index:previous)if(mapping[index]>=0)nextOrder.push_back(mapping[index]);
    auto at=std::find(nextOrder.begin(),nextOrder.end(),nextCurrent);nextOrder.insert(at+1,insert);
    queue=std::move(updated);current=nextCurrent;order.restore(nextOrder,current);return true;
}
}