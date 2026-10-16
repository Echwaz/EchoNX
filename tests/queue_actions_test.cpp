#include "queue_actions.hpp"
#include <cassert>
#include <iostream>
int main(){
    auto song=[](const char* id){echo::Song s;s.id=id;s.title=id;return s;};
    echo::PlaybackOrder order(42);std::vector<echo::Song> queue={song("a"),song("b"),song("c")};int current=1;
    assert(echo::enqueue_next(queue,current,song("a"),order));assert(current==0&&queue[current].id=="b"&&queue[current+1].id=="a"&&queue.size()==3);assert(order.step(1)==1);
    order.shuffle=true;assert(echo::enqueue_next(queue,current,song("d"),order));assert(queue[current].id=="b"&&queue.size()==4&&queue[current+1].id=="d");assert(order.step(1,true)==current+1);
    assert(echo::enqueue_next(queue,current,song("c"),order));assert(queue.size()==4&&queue[current+1].id=="c");assert(order.step(1)==current+1);
    auto before=queue.size();assert(!echo::enqueue_next(queue,current,song("b"),order));assert(queue.size()==before&&queue[current].id=="b");
    std::vector<echo::Song> empty;int invalid=-1;assert(!echo::enqueue_next(empty,invalid,song("a"),order));
    queue={song("a"),song("b"),song("c"),song("d"),song("e")};current=2;
    order.shuffle=true;assert(order.restore({0,1,2,4,3},current));
    assert(echo::enqueue_next(queue,current,song("f"),order));
    auto ids=[&]{std::vector<std::string> result;for(int i:order.upcoming())result.push_back(queue[i].id);return result;};
    assert((ids()==std::vector<std::string>{"f","e","d"}));
    assert(echo::enqueue_next(queue,current,song("d"),order));
    assert((ids()==std::vector<std::string>{"d","f","e"}));
    assert(order.step(-1)>=0&&queue[order.step(-1)].id=="a");
    std::cout<<"PASS: play next inserts/moves without duplicates, preserves current song, works with shuffle, and rejects current/empty queues.\n";
}