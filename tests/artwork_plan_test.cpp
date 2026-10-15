#include "artwork_plan.hpp"
#include <cassert>
#include <set>
#include <iostream>
int main(){
    auto plan=echo::artwork_plan(100,50);assert(plan.size()==43);for(int i=0;i<7;i++)assert(plan[i]==50+i);assert(plan[7]==57&&plan[30]==80&&plan[31]==49&&plan.back()==38);std::set<int> unique(plan.begin(),plan.end());assert(unique.size()==plan.size());
    assert(echo::artwork_plan(100,0).size()==31);assert(echo::artwork_plan(100,99).size()==13);assert(echo::artwork_plan(0,0).empty());assert(echo::artwork_plan(1,500)==std::vector<int>{0});
    for(int first=-2;first<130;first++){auto rows=echo::artwork_plan(120,first);for(int i:rows)assert(i>=0&&i<120);assert(rows.size()<=43);}
    assert(echo::thumbnailLimit*echo::thumbnailSize*echo::thumbnailSize*4<5*1024*1024);
    std::cout<<"PASS: visible-first prefetch, 24 ahead/12 behind, boundary clamping, no duplicates, thumbnail pixel budget below 5 MiB.\n";
}