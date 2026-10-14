#include "model.hpp"
#include <cassert>
#include <iostream>
int main(){
    std::vector<echo::Song> tracks={{"one","One","Artist"},{"two","Two","Artist","track-cover"}};
    echo::inherit_album_cover(tracks,"album-cover");
    assert(tracks[0].cover=="album-cover");
    assert(tracks[1].cover=="track-cover");
    echo::inherit_album_cover(tracks,"other-cover");
    assert(tracks[0].cover=="album-cover");
    std::cout<<"PASS: missing album track covers inherit artwork; existing covers are preserved.\n";
}