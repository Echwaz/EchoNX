#pragma once
#include <algorithm>
#include <vector>
namespace echo {
constexpr int artworkVisibleRows=7,artworkAhead=24,artworkBehind=12,thumbnailLimit=128,thumbnailSize=96;
inline std::vector<int> artwork_plan(int count,int first){
    std::vector<int> order;if(count<=0)return order;first=std::clamp(first,0,count-1);
    for(int i=first;i<std::min(count,first+artworkVisibleRows);i++)order.push_back(i);
    for(int i=first+artworkVisibleRows;i<std::min(count,first+artworkVisibleRows+artworkAhead);i++)order.push_back(i);
    for(int i=first-1;i>=std::max(0,first-artworkBehind);i--)order.push_back(i);
    return order;
}
}