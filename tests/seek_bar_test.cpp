#include "seek_bar.hpp"
#include <cassert>
#include <iostream>
int main(){
    echo::SeekBarState bar;bar.configure(100,200,400,"a",200,20,100);assert(!bar.begin(300,100));assert(bar.begin(300,210));assert(bar.displayTime(20)==100);double visual=bar.advance(20,116);assert(visual>.1&&visual<.5&&bar.emphasis()>0);bar.move(500);auto target=bar.release("a",1000);assert(target&&*target==199.9);assert(!bar.release("a",1001));assert(bar.displayTime(20)==200);bar.advance(20,1020);assert(bar.displayTime(20)==200);bar.advance(199.9,1040);assert(bar.displayTime(199.9)==199.9);
    assert(bar.begin(300,200));bar.move(-500);target=bar.release("a",1100);assert(target&&*target==0);
    assert(bar.begin(300,200));bar.configure(100,200,400,"b",100,0,1200);assert(!bar.release("b",1201));assert(!bar.dragging());
    bar.configure(100,200,400,"b",0,0,1300);assert(!bar.begin(300,200));bar.configure(100,200,400,"b",100,0,1400);assert(bar.begin(300,200));bar.cancel();assert(!bar.release("b",1500));
    std::cout<<"PASS: expanded hit area, drag preview, smooth interpolation, single release seek, clamping, pending feedback, track changes, cancellation and unknown duration.\n";
}