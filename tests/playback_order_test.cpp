#include "playback_order.hpp"
#include <cassert>
#include <set>
#include <iostream>
int main(){
    echo::PlaybackOrder order(42);order.reset(3,0);assert(order.step(1)==1);assert(order.step(-1)==0);assert(order.step(-1)==-1);
    order.repeat=1;assert(order.step(-1)==2);assert(order.step(1)==0);
    order.repeat=2;assert(order.step(1,true)==0);assert(order.step(1,false)==1);
    order.shuffle=true;order.repeat=0;order.reset(8,3);std::set<int> seen{3};int previous=3;
    for(int n=0;n<7;n++){int next=order.step(1);assert(next>=0&&seen.insert(next).second);assert(order.step(-1)==previous);assert(order.step(1)==next);previous=next;}
    assert(order.step(1)==-1);order.repeat=1;assert(order.step(1)==3);
    order.reset(1,0);assert(order.step(1)==0);order.repeat=0;assert(order.step(1)==-1);
    order.reset(0,0);assert(order.step(1)==-1);
    order.shuffle=false;order.repeat=0;order.reset(10,4);assert((order.upcoming()==std::vector<int>{5,6,7,8,9}));assert(order.select(7));assert((order.upcoming()==std::vector<int>{8,9}));assert(order.step(1)==8);
    order.shuffle=true;order.reset(10,4);auto upcoming=order.upcoming();assert(upcoming.size()==9);assert(std::find(upcoming.begin(),upcoming.end(),4)==upcoming.end());for(int index:upcoming)assert(order.step(1)==index);assert(order.upcoming().empty());
    order.repeat=1;order.shuffle=false;order.reset(3,2);assert((order.upcoming()==std::vector<int>{0,1}));
    order.repeat=0;order.shuffle=false;order.reset(5,0);order.promoteNext(3);assert(order.step(1)==3);assert((order.upcoming()==std::vector<int>{1,2,4}));assert(order.step(1)==1);
    order.repeat=0;order.shuffle=false;order.reset(4,0);auto original=order.displayOrder();assert(order.select(2));assert(order.displayOrder()==original);assert(order.step(1)==3);assert(order.select(0));assert(order.displayOrder()==original);order.shuffle=true;order.reset(8,0);original=order.displayOrder();assert(order.select(original[4]));assert(order.displayOrder()==original);assert(order.step(1)==original[5]);
    std::cout<<"PASS: sequential navigation, shuffle without repeats, previous history, repeat all/one, manual skip, empty/single queues.\n";
}