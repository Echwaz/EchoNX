#pragma once
#include <algorithm>
#include <numeric>
#include <random>
#include <vector>
namespace echo {
class PlaybackOrder {
    std::vector<int> order_;
    int position_=0;
    std::mt19937 random_;
public:
    bool shuffle=false;
    int repeat=0;
    explicit PlaybackOrder(unsigned seed=1):random_(seed){}
    void reset(int size,int current){
        order_.resize(size);std::iota(order_.begin(),order_.end(),0);
        if(size==0){position_=0;return;}
        current=std::clamp(current,0,size-1);
        if(shuffle){std::swap(order_[0],order_[current]);std::shuffle(order_.begin()+1,order_.end(),random_);position_=0;}else position_=current;
    }
    void promoteNext(int index){
        if(order_.empty()||index<0||index>=size()||order_[position_]==index)return;
        int current=order_[position_];auto found=std::find(order_.begin(),order_.end(),index);
        order_.erase(found);position_=int(std::find(order_.begin(),order_.end(),current)-order_.begin());
        order_.insert(order_.begin()+position_+1,index);
    }
    std::vector<int> upcoming()const{
        if(order_.empty())return {};
        std::vector<int> result(order_.begin()+position_+1,order_.end());
        if(repeat==1)result.insert(result.end(),order_.begin(),order_.begin()+position_);
        return result;
    }
    bool select(int index){auto it=std::find(order_.begin(),order_.end(),index);if(it==order_.end())return false;position_=int(it-order_.begin());return true;}
    const std::vector<int>& displayOrder()const{return order_;}
    int size()const{return int(order_.size());}
    int step(int direction,bool automatic=false){
        if(order_.empty())return -1;
        if(automatic&&repeat==2)return order_[position_];
        int next=position_+direction;
        if(next<0||next>=size()){if(repeat==1)next=(next+size())%size();else return -1;}
        position_=next;return order_[position_];
    }
};
}