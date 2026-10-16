#define main embedded_main
#include "../src/app.cpp"
#undef main
#include <cassert>
#include <iostream>
int main(){
    std::filesystem::create_directories("tests");for(const auto* name:{"home.json","related.json"})std::ofstream(std::string("tests/")+name)<<"{}";
    echo::App app;app.applyPending();app.tab_=2;app.selected_=4;app.listScroll_.offset=210;
    echo::App::FetchResult home;home.job.mode=0;home.job.revision=app.fetchRevisions_[0];home.rows={{"home-new","New"}};app.pendingFetch_.push_back(home);app.applyPending();assert(app.selected_==4&&app.listScroll_.offset==210);
    app.current_.id="B";echo::App::FetchResult related;related.job.mode=2;related.job.value="A";related.job.revision=app.fetchRevisions_[2];related.rows={{"old","Old"}};app.pendingFetch_.push_back(related);app.applyPending();assert(app.relatedFor_!="A");
    app.query_="new";echo::App::FetchResult search;search.job.mode=1;search.job.value="old";search.job.revision=app.fetchRevisions_[1];search.rows={{"old","Old"}};app.pendingFetch_.push_back(search);app.applyPending();assert(app.searchRows_.empty());
    echo::Song a,b;a.id="art-a";b.id="art-b";a.cover=b.cover="shared-cover";
    auto* surface=SDL_CreateRGBSurfaceWithFormat(0,32,32,32,SDL_PIXELFORMAT_RGBA32);assert(surface);echo::App::PreparedArt art;art.key=a.cover;art.image=art.thumbnail=std::shared_ptr<SDL_Surface>(surface,SDL_FreeSurface);app.decodedArt_[a.cover]={art,1};app.fullArtWanted_.insert(a.id);
    assert(app.reuseArtwork(a));assert(app.reuseArtwork(b));assert(app.artPending_[a.id].image&&!app.artPending_[b.id].image&&app.artPending_[b.id].thumbnail);
    auto first=app.artworkTexture(app.thumbnailGpu_,a.cover,surface);auto second=app.artworkTexture(app.thumbnailGpu_,b.cover,surface);assert(first==second);first.reset();second.reset();app.artPending_.clear();app.decodedArt_.clear();
    auto* font=app.fonts_.at(22);echo::TextCache cache;cache.beginFrame();auto* entry=cache.get(app.renderer_,font,"reused");assert(entry);auto* texture=entry->texture;echo::TextCache::tint(*entry,{20,30,40,90});cache.beginFrame();assert(cache.get(app.renderer_,font,"reused")->texture==texture);
    for(int n=0;n<270;++n){cache.beginFrame();cache.get(app.renderer_,font,"entry-"+std::to_string(n));cache.get(app.renderer_,font,"reused");}assert(cache.size()<=256&&cache.get(app.renderer_,font,"reused")->texture==texture);cache.clear();
    app.playlists_={{"test","Test",{a}}};assert(app.commitPlaylists());auto ticket=app.playlistTicket_;for(int n=0;n<500&&app.playlistWriter_->completed()<ticket;++n)SDL_Delay(2);assert(app.playlistWriter_->completed()==ticket);
    std::cout<<"PASS: offscreen fetch preserves viewport, stale search/related rejected, shared art surfaces/textures, thumbnail reuse, text LRU and asynchronous playlist writes.\n";
}
