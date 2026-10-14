#include "player.hpp"
#include "recommendations.hpp"
#include "library_store.hpp"
#include "touch_scroll.hpp"
#include "visuals.hpp"
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <fstream>
#include <map>
#include <set>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <condition_variable>
#ifdef __SWITCH__
#include <switch.h>
#include <curl/curl.h>
extern "C" void userAppInit(){socketInitializeDefault();nifmInitialize(NifmServiceType_User);romfsInit();plInitialize(PlServiceType_User);lblInitialize();}
extern "C" void userAppExit(){lblSwitchBacklightOn(0);lblExit();plExit();romfsExit();nifmExit();socketExit();}
#endif

namespace echo {
constexpr SDL_Color ink{235,236,241,255},muted{174,176,185,255},paper{18,19,22,255},surface{32,34,39,255},accent{235,236,241,255};
const char* tabNames[]={"Home","Search","Library","Queue","Artist","Album"};
std::string clockText(double sec){int t=std::max(0,int(sec));return std::to_string(t/60)+":"+(t%60<10?"0":"")+std::to_string(t%60);}
struct Hit {SDL_Rect rect;std::function<void()> action;};
class App {
    SDL_Window* window_=nullptr;SDL_Renderer* renderer_=nullptr;std::map<int,TTF_Font*> fonts_;
    std::map<std::string,SDL_Texture*> textures_,backgrounds_;std::vector<Hit> hits_;
    std::map<std::string,SDL_Texture*> shapeTextures_;
    struct TitleTexture{SDL_Texture* texture;int width,height;Uint32 started;};std::map<std::string,TitleTexture> titleTextures_;
    std::string lyricAnimationSong_;int lyricTarget_=-2;double lyricVisual_=0,lyricFrom_=0;Uint32 lyricAnimationStart_=0;std::map<std::string,int> lyricHeights_;Song pendingMetadata_;
    Api api_;std::unique_ptr<Player> player_;std::thread fetch_,art_,lyricsWorker_;
    std::atomic<bool> closing_{false},loading_{false};std::mutex dataMutex_;
    std::vector<Song> results_,searchRows_,artistRows_,albumRows_,relatedRows_,pending_,library_,queue_,artRequests_;std::map<std::string,std::string> artPending_;
    std::condition_variable lyricsCv_;Song lyricsRequest_;bool lyricsRequested_=false;std::map<std::string,Lyrics> lyricCache_,lyricPending_;
    TouchScroll listScroll_,lyricScroll_;float lyricScrollMax_=0;std::string lyricScrollSong_;bool fullscreen_=false,pointerActive_=false,pointerOnList_=false,pointerOnLyrics_=false,wakeTouch_=false;int pointerY_=0;SDL_FingerID pointerFinger_=0;
    std::condition_variable artCv_;bool pendingReady_=false;int pendingMode_=0,deferredMode_=-1;std::string deferredValue_;
    std::string notice_,pendingNotice_,heading_="Discover",query_;int tab_=0,selected_=0,queueIndex_=-1;
    Song current_,artistAnchor_,albumAnchor_;bool running_=true,screenOff_=false,focusHooked_=false;int pointerX_=0;size_t homeOffset_=0;std::string pendingArtistKey_;int artworkStart_=-1,artworkTab_=-1;
#ifdef __SWITCH__
    PadState pad_;AppletHookCookie hook_{};bool audioControlReady_=false;
#endif
    Uint32 audioPoll_=0;std::string audioRoute_="Audio: --";
    Uint32 artworkRetry_=0,analogRepeat_=0;int analogDirection_=0;
    std::string libraryPath_;
    void color(SDL_Color c){SDL_SetRenderDrawColor(renderer_,c.r,c.g,c.b,c.a);}
    void rect(SDL_Rect r,SDL_Color c){color(c);SDL_RenderFillRect(renderer_,&r);}
    void round(SDL_Rect r,int radius,SDL_Color c){
        std::string key="round|"+std::to_string(r.w)+"|"+std::to_string(r.h)+"|"+std::to_string(radius)+"|"+std::to_string(c.r)+"|"+std::to_string(c.g)+"|"+std::to_string(c.b);auto it=shapeTextures_.find(key);if(it==shapeTextures_.end()){auto* s=smooth_shape(r.w,r.h,c,[&](double x,double y){double dx=x-std::clamp(x,double(radius),double(r.w-radius)),dy=y-std::clamp(y,double(radius),double(r.h-radius));return dx*dx+dy*dy<=radius*radius;});if(!s)return;auto* t=SDL_CreateTextureFromSurface(renderer_,s);SDL_FreeSurface(s);if(!t)return;it=shapeTextures_.emplace(key,t).first;}SDL_RenderCopy(renderer_,it->second,nullptr,&r);
    }
    void label(std::string value,int x,int y,int size,SDL_Color c,int width=1000){
        if(value.empty())return;auto* f=fonts_.at(size);int w=0,h=0;TTF_SizeUTF8(f,value.c_str(),&w,&h);
        if(w>width){while(value.size()>1){size_t cut=value.size()-1;while(cut>0 && (static_cast<unsigned char>(value[cut])&0xc0)==0x80)--cut;value.resize(cut);std::string clipped=value+"...";TTF_SizeUTF8(f,clipped.c_str(),&w,&h);if(w<=width){value=clipped;break;}}}
        SDL_Surface* s=TTF_RenderUTF8_Blended(f,value.c_str(),c);if(!s)return;
        SDL_Texture* t=SDL_CreateTextureFromSurface(renderer_,s);SDL_Rect dest{x,y,s->w,s->h};SDL_RenderCopy(renderer_,t,nullptr,&dest);SDL_DestroyTexture(t);SDL_FreeSurface(s);
    }
    void centeredLabel(const std::string& title,SDL_Rect r,int size,SDL_Color fg){int width=0,height=0;TTF_SizeUTF8(fonts_.at(size),title.c_str(),&width,&height);label(title,r.x+(r.w-std::min(width,r.w-16))/2,r.y+(r.h-height)/2,size,fg,r.w-16);}
    void button(SDL_Rect r,const std::string& title,std::function<void()> action,bool primary=false,int size=22){round(r,14,primary?ink:surface);centeredLabel(title,r,size,primary?paper:ink);hits_.push_back({r,std::move(action)});}
    void songTitle(const std::string& value,int x,int y,int size,int width){
        if(value.empty())return;std::string key=std::to_string(size)+"|"+std::to_string(width)+"|"+value;auto found=titleTextures_.find(key);
        if(found==titleTextures_.end()){if(titleTextures_.size()>=128){SDL_DestroyTexture(titleTextures_.begin()->second.texture);titleTextures_.erase(titleTextures_.begin());}auto* s=TTF_RenderUTF8_Blended(fonts_.at(size),value.c_str(),ink);if(!s)return;TitleTexture entry{SDL_CreateTextureFromSurface(renderer_,s),s->w,s->h,SDL_GetTicks()};SDL_FreeSurface(s);found=titleTextures_.emplace(key,entry).first;}
        const auto& entry=found->second;SDL_Rect oldClip{},clip{x,y,width,entry.height};bool clipped=SDL_RenderIsClipEnabled(renderer_);if(clipped){SDL_RenderGetClipRect(renderer_,&oldClip);SDL_Rect intersection{};if(!SDL_IntersectRect(&clip,&oldClip,&intersection))return;clip=intersection;}SDL_RenderSetClipRect(renderer_,&clip);
        bool scrolling=entry.width>width;int offset=scrolling?int(title_scroll(entry.width,(SDL_GetTicks()-entry.started)/1000.0)):0;SDL_Rect dest{x-offset,y,entry.width,entry.height};SDL_RenderCopy(renderer_,entry.texture,nullptr,&dest);if(scrolling){dest.x+=entry.width+64;SDL_RenderCopy(renderer_,entry.texture,nullptr,&dest);}SDL_RenderSetClipRect(renderer_,clipped?&oldClip:nullptr);
    }
    void triangle(Triangle shape,int y,SDL_Color c){std::string key="triangle|"+std::to_string(shape.width)+"|"+std::to_string(shape.height)+"|"+std::to_string(shape.right)+"|"+std::to_string(c.r)+"|"+std::to_string(c.g)+"|"+std::to_string(c.b);auto it=shapeTextures_.find(key);if(it==shapeTextures_.end()){auto* s=smooth_shape(shape.width,shape.height*2,c,[&](double x,double yy){double reach=shape.width*(1-std::abs(yy-shape.height)/shape.height);return shape.right?x<=reach:x>=shape.width-reach;});if(!s)return;auto* t=SDL_CreateTextureFromSurface(renderer_,s);SDL_FreeSurface(s);if(!t)return;it=shapeTextures_.emplace(key,t).first;}SDL_Rect dest{shape.left,y-shape.height,shape.width,shape.height*2};SDL_RenderCopy(renderer_,it->second,nullptr,&dest);}
    void icon(SDL_Rect r,int kind,std::function<void()> action,bool primary=false){
        if(primary)round(r,r.w/2,ink);SDL_Color fg=primary?paper:ink;int cx=r.x+r.w/2,cy=r.y+r.h/2;
        if(kind==0||kind==3){triangle(skip_shape(cx,kind==3),cy,fg);rect({cx+(kind==0?-20:16),cy-13,4,26},fg);}
        else if(kind==1)triangle(play_shape(cx),cy,fg);
        else if(kind==2){rect({cx-11,cy-15,7,30},fg);rect({cx+4,cy-15,7,30},fg);}
        else if(kind==4){color(fg);for(int t=0;t<3;t++){SDL_RenderDrawLine(renderer_,cx-10,cy-5+t,cx,cy+5+t);SDL_RenderDrawLine(renderer_,cx,cy+5+t,cx+10,cy-5+t);}}
        hits_.push_back({r,std::move(action)});
    }
    void togglePlayback(){if(current_.id.empty())search();else if(!player_->active())player_->play(current_);else player_->toggle();}
    void transport(int center,int y){icon({center-137,y+10,64,64},0,[this]{next(-1);});icon({center-42,y,84,84},player_->active()&&!player_->paused()?2:1,[this]{togglePlayback();},true);icon({center+73,y+10,64,64},3,[this]{next(1);});}
    void focusPlayer(){if(current_.id.empty()){search();return;}fullscreen_=true;requestLyrics(current_,true);}
    void requestLyrics(const Song& song,bool retry=false){auto found=lyricCache_.find(song.id);if(found!=lyricCache_.end()&&(!retry||!found->second.lines.empty()||!found->second.plain.empty()||found->second.status=="Instrumental"))return;lyricCache_[song.id]={{},{},"Loading lyrics..."};
#ifdef ECHO_PREVIEW
        lyricCache_[song.id]={{},{},"Lyrics appear here when available"};
#else
        {std::lock_guard<std::mutex> lock(dataMutex_);lyricsRequest_=song;lyricsRequested_=true;}lyricsCv_.notify_one();
#endif
    }
    void cover(const Song& s,SDL_Rect r){auto it=textures_.find(s.id);if(it!=textures_.end())SDL_RenderCopy(renderer_,it->second,nullptr,&r);else{round(r,16,surface);if(r.w>100)label(s.title.empty()?"Music":s.title,r.x+20,r.y+r.h/2-16,28,muted,r.w-40);}}
    std::vector<Song>& visible(){return tab_==2?library_:tab_==3?queue_:tab_==4?artistRows_:tab_==5?albumRows_:tab_==1?searchRows_:results_;}
    void saveLibrary(){save_library(libraryPath_,library_);}
    void favorite(){if(current_.id.empty()){notice_="Choose a song before saving it to your library.";return;}auto previous=library_;auto it=std::find_if(library_.begin(),library_.end(),[&](const Song& s){return s.id==current_.id;});if(it==library_.end())library_.push_back(current_);else library_.erase(it);try{saveLibrary();homeOffset_=0;fetch(0);notice_="Library updated";}catch(const std::exception& e){library_=std::move(previous);notice_=e.what();}}
    static std::string pageKey(int mode,const Song& song){return mode==4?song.albumId+"|"+song.id:song.artistId+"|"+song.artist;}
    void fetch(int mode,const std::string& value=""){
        if(loading_){deferredMode_=mode;deferredValue_=value;return;}
        if(fetch_.joinable())fetch_.join();loading_=true;auto saved=library_;auto anchor=mode==4?albumAnchor_:artistAnchor_;auto playing=current_;size_t offset=homeOffset_;if(mode==0)homeOffset_++;
#ifdef ECHO_PREVIEW
        const char* path=mode==4?"tests/album.json":mode==3?"tests/artist-songs.json":mode==2?"tests/related.json":mode==1?"tests/search.json":"tests/home.json";
        std::ifstream f(path);Json j;f>>j;pending_=songs(j,mode>=3?120:40);if(mode==0&&!saved.empty()){std::ifstream r("tests/related.json");Json related;r>>related;pending_=rank_recommendations(saved,{songs(related)});}pendingNotice_="";pendingArtistKey_=pageKey(mode,anchor);pendingMode_=mode;pendingReady_=true;loading_=false;
#else
        fetch_=std::thread([this,mode,value,saved,anchor,offset,playing]{
            try{auto rows=mode==4?api_.album(anchor):mode==3?api_.artist(anchor):mode==2?api_.related(value):mode==1?api_.search(value):api_.for_saved(saved,offset);
                Song metadata;
                if(mode==2){for(const auto& s:rows)if(s.id==value)metadata=s;
                    if(playing.id==value&&playing.album.empty()&&metadata.album.empty())try{for(const auto& s:api_.search(playing.title+" "+playing.artist))if(s.id==value){metadata=s;break;}}catch(...){}
                    rows.erase(std::remove_if(rows.begin(),rows.end(),[&](const Song& s){return s.id==value;}),rows.end());}
                std::lock_guard<std::mutex> l(dataMutex_);pendingMetadata_=metadata;pending_=std::move(rows);pendingArtistKey_=pageKey(mode,anchor);pendingMode_=mode;pendingReady_=true;pendingNotice_=pending_.empty()?"No songs returned. X searches, or tap Refresh to retry.":"";
            }catch(const std::exception& e){std::lock_guard<std::mutex> l(dataMutex_);pending_.clear();pendingArtistKey_=pageKey(mode,anchor);pendingMode_=mode;pendingReady_=true;pendingNotice_=std::string(e.what())+"  Tap Refresh to retry.";}
            loading_=false;
        });
#endif
    }
    void artwork(const std::vector<Song>& items){
#ifdef ECHO_PREVIEW
        for(const auto& s:items){std::ifstream f("tests/art/"+s.id+".img",std::ios::binary);if(f)artPending_[s.id]=std::string(std::istreambuf_iterator<char>(f),{});}
#else
        {std::lock_guard<std::mutex> l(dataMutex_);artRequests_.clear();for(const auto& s:items)if(!textures_.count(s.id) && !s.cover.empty())artRequests_.push_back(s);}artCv_.notify_one();
#endif
    }
    void applyPending(){
        bool update=false;std::vector<Song> items;
        {std::lock_guard<std::mutex> l(dataMutex_);
            if(pendingReady_){auto& target=pendingMode_==4?albumRows_:pendingMode_==3?artistRows_:pendingMode_==2?relatedRows_:pendingMode_==1?searchRows_:results_;const auto& anchor=pendingMode_==4?albumAnchor_:artistAnchor_;if((pendingMode_!=3&&pendingMode_!=4)||pendingArtistKey_==pageKey(pendingMode_,anchor)){target=std::move(pending_);if(pendingMode_!=2){selected_=0;listScroll_.reset();}update=true;items=target;if(!current_.id.empty())items.insert(items.begin(),current_);notice_=pendingNotice_;}pendingReady_=false;pendingNotice_.clear();}
            if(!pendingMetadata_.id.empty()){auto enrich=[&](Song& s){if(s.id!=pendingMetadata_.id)return;if(s.album.empty())s.album=pendingMetadata_.album;if(s.albumId.empty())s.albumId=pendingMetadata_.albumId;};enrich(current_);for(auto& s:queue_)enrich(s);for(auto& s:artistRows_)enrich(s);pendingMetadata_={};}
            for(auto& [id,lyrics]:lyricPending_)lyricCache_[id]=std::move(lyrics);lyricPending_.clear();
            for(auto& [id,bytes]:artPending_){if(textures_.count(id))continue;SDL_RWops* rw=SDL_RWFromConstMem(bytes.data(),int(bytes.size()));SDL_Surface* s=IMG_Load_RW(rw,1);if(s){auto* texture=SDL_CreateTextureFromSurface(renderer_,s);if(texture){textures_[id]=texture;backgrounds_[id]=album_blur(renderer_,s,.25f);}SDL_FreeSurface(s);}}
            artPending_.clear();
            std::set<std::string> keep{current_.id};auto& shown=visible();int first=int(listScroll_.offset)/72;for(int i=first;i<int(shown.size())&&i<first+7;i++)keep.insert(shown[i].id);
            while(textures_.size()>32){auto it=std::find_if(textures_.begin(),textures_.end(),[&](const auto& item){return !keep.count(item.first);});if(it==textures_.end())break;SDL_DestroyTexture(it->second);auto bg=backgrounds_.find(it->first);if(bg!=backgrounds_.end()){SDL_DestroyTexture(bg->second);backgrounds_.erase(bg);}textures_.erase(it);}
            while(lyricCache_.size()>32){auto it=lyricCache_.begin();if(it->first==current_.id)++it;if(it==lyricCache_.end())break;lyricCache_.erase(it);}if(lyricHeights_.size()>2048)lyricHeights_.clear();
        }
        if(update){if(items.size()>12)items.resize(12);artwork(items);}
        if(!loading_ && deferredMode_>=0){int mode=deferredMode_;auto value=deferredValue_;deferredMode_=-1;fetch(mode,value);}
    }
    void choose(int i){auto rows=visible();if(i<0||i>=int(rows.size()))return;
        if(tab_!=3){queue_=rows;queueIndex_=i;}else queueIndex_=i;
        current_=queue_[queueIndex_];artwork({current_});player_->play(current_);fetch(2,current_.id);requestLyrics(current_);
    }
    void next(int direction){if(queue_.empty()){notice_="Choose a song to start the queue.";return;}int i=queueIndex_+direction;if(i<0)i=0;
        if(i>=int(queue_.size())){std::set<std::string> ids;for(auto& s:queue_)ids.insert(s.id);for(auto& s:relatedRows_)if(ids.insert(s.id).second)queue_.push_back(s);}
        if(i<int(queue_.size())){queueIndex_=i;current_=queue_[i];artwork({current_});player_->play(current_);fetch(2,current_.id);requestLyrics(current_);}else notice_="Queue finished. Pick another song.";
    }
    void openArtist(Song song={}){if(song.id.empty()){auto& rows=visible();if(selected_<int(rows.size()))song=rows[selected_];else song=current_;}if(song.artist.empty()){notice_="Choose a song with an artist first.";return;}artistAnchor_=song;artistRows_.clear();fullscreen_=false;tab_=4;selected_=0;listScroll_.reset();fetch(3);}
    void openAlbum(Song song={}){if(song.id.empty()){auto& rows=visible();if(selected_<int(rows.size()))song=rows[selected_];else song=current_;}if(song.id.empty()){notice_="Choose a song first.";return;}albumAnchor_=song;albumRows_.clear();fullscreen_=false;tab_=5;selected_=0;listScroll_.reset();fetch(4);}
    void changeTab(int t){int next=(t+6)%6;if(next==4&&artistAnchor_.id.empty()){openArtist();return;}if(next==5&&albumAnchor_.id.empty()){openAlbum();return;}fullscreen_=false;tab_=next;selected_=0;listScroll_.reset();if(tab_==0){heading_="Discover";fetch(0);}if(tab_==1 && query_.empty())notice_="Press X or tap Search to enter a song or artist.";if(tab_==2)artwork(library_);}
    void search(){
        std::string q;
#ifdef __SWITCH__
        SwkbdConfig keyboard{};Result rc=swkbdCreate(&keyboard,0);if(R_FAILED(rc)){notice_="Keyboard unavailable";return;}swkbdConfigMakePresetDefault(&keyboard);swkbdConfigSetHeaderText(&keyboard,"YouTube Music");swkbdConfigSetGuideText(&keyboard,"Song or artist");swkbdConfigSetStringLenMax(&keyboard,120);char buffer[512]{};rc=swkbdShow(&keyboard,buffer,sizeof(buffer));swkbdClose(&keyboard);if(R_FAILED(rc))return;q=buffer;
#else
        q="Daft Punk";
#endif
        if(q.empty())return;query_=q;tab_=1;heading_="Search: "+q;selected_=0;fetch(1,q);
    }
    void wake(){if(screenOff_){
#ifdef __SWITCH__
        if(R_FAILED(lblSwitchBacklightOn(0)))return;
#endif
        screenOff_=false;}}
    void dark(){if(!player_->active() || player_->paused()){notice_="Start music before turning off the screen.";return;}
#ifdef __SWITCH__
        if(R_FAILED(appletSetMediaPlaybackState(true)) || R_FAILED(lblSwitchBacklightOff(0))){notice_="Screen off unavailable";return;}
#endif
        screenOff_=true;
    }
    void action(int key){if(screenOff_){wake();return;}
        auto& rows=visible();switch(key){
        case 0:if(fullscreen_)togglePlayback();else choose(selected_);break;case 1:if(fullscreen_)fullscreen_=false;else changeTab(0);break;case 2:fullscreen_=false;search();break;case 3:togglePlayback();break;
        case 4:if(selected_>0)--selected_;break;case 5:if(selected_+1<int(rows.size()))++selected_;break;
        case 6:changeTab(tab_-1);break;case 7:changeTab(tab_+1);break;case 8:next(-1);break;case 9:next(1);break;
        case 10:dark();break;case 11:favorite();break;case 12:running_=false;break;
        case 13:openArtist();break;
        }
        if(key==4||key==5){float top=selected_*72.f;if(top<listScroll_.offset)listScroll_.offset=top;if(top+66>listScroll_.offset+390)listScroll_.offset=top+66-390;}
    }
public:
    App(){
        SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS,"0");SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS,"0");
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"linear");
        if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_EVENTS)!=0)throw std::runtime_error(SDL_GetError());
        if(TTF_Init()!=0)throw std::runtime_error(TTF_GetError());IMG_Init(IMG_INIT_JPG|IMG_INIT_PNG);
        window_=SDL_CreateWindow("EchoNX",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1280,720,0);
        if(!window_)throw std::runtime_error(SDL_GetError());
        renderer_=SDL_CreateRenderer(window_,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
        if(!renderer_)renderer_=SDL_CreateRenderer(window_,-1,SDL_RENDERER_SOFTWARE);
        if(!renderer_)throw std::runtime_error(SDL_GetError());SDL_RenderSetLogicalSize(renderer_,1280,720);SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_BLEND);
        for(int size:{18,22,28,36,44}){
#ifdef __SWITCH__
            PlFontData font{};if(R_FAILED(plGetSharedFontByType(&font,PlSharedFontType_Standard)))throw std::runtime_error("System font unavailable");fonts_[size]=TTF_OpenFontRW(SDL_RWFromConstMem(font.address,font.size),1,size);
#else
            const char* previewFont=std::getenv("ECHONX_FONT");fonts_[size]=TTF_OpenFont(previewFont?previewFont:"font.ttf",size);
#endif
            if(!fonts_[size])throw std::runtime_error(TTF_GetError());}
        player_=std::make_unique<Player>(api_);
#ifndef ECHO_PREVIEW
        art_=std::thread([this]{while(!closing_){Song s;{std::unique_lock<std::mutex> l(dataMutex_);artCv_.wait(l,[this]{return closing_ || !artRequests_.empty();});if(closing_)break;s=artRequests_.front();artRequests_.erase(artRequests_.begin());}try{auto bytes=http(s.cover,"",{},&closing_,browserUA,2*1024*1024);std::lock_guard<std::mutex> l(dataMutex_);artPending_[s.id]=std::move(bytes);}catch(...) {}}});
        lyricsWorker_=std::thread([this]{while(!closing_){Song song;{std::unique_lock<std::mutex> lock(dataMutex_);lyricsCv_.wait(lock,[this]{return closing_||lyricsRequested_;});if(closing_)break;song=lyricsRequest_;lyricsRequested_=false;}Lyrics lyrics;try{lyrics=api_.lyrics(song,&closing_);}catch(...){lyrics.status="Lyrics unavailable right now. Tap here to retry.";}std::lock_guard<std::mutex> lock(dataMutex_);lyricPending_[song.id]=std::move(lyrics);}});
#endif
#ifdef __SWITCH__
        libraryPath_="sdmc:/switch/echonx/library.json";padConfigureInput(1,HidNpadStyleSet_NpadStandard);padInitializeDefault(&pad_);
        appletHook(&hook_,[](AppletHookType hook,void* ctx){if(hook==AppletHookType_OnFocusState && appletGetFocusState()!=AppletFocusState_InFocus)static_cast<App*>(ctx)->wake();},this);focusHooked_=true;
#else
        libraryPath_="tests/local-library.json";
#endif
        library_=load_library(libraryPath_);
        fetch(0);
    }
    ~App(){closing_=true;artCv_.notify_one();lyricsCv_.notify_one();wake();player_.reset();if(fetch_.joinable())fetch_.join();if(art_.joinable())art_.join();if(lyricsWorker_.joinable())lyricsWorker_.join();
#ifdef __SWITCH__
        if(focusHooked_)appletUnhook(&hook_);appletSetMediaPlaybackState(false);if(audioControlReady_)audctlExit();
#endif
        for(auto& p:shapeTextures_)SDL_DestroyTexture(p.second);for(auto& p:titleTextures_)SDL_DestroyTexture(p.second.texture);for(auto& p:textures_)SDL_DestroyTexture(p.second);for(auto& p:backgrounds_)SDL_DestroyTexture(p.second);for(auto& p:fonts_)TTF_CloseFont(p.second);SDL_DestroyRenderer(renderer_);SDL_DestroyWindow(window_);IMG_Quit();TTF_Quit();SDL_Quit();}
    void timeline(int x,int y,int width){double duration=player_->duration(),position=player_->position();rect({x,y,width,5},surface);if(duration>0)rect({x,y,int(width*std::min(1.0,position/duration)),5},accent);hits_.push_back({{x,y-20,width,45},[this,duration,x,width]{player_->seek(duration*std::clamp((pointerX_-x)/double(width),0.0,1.0));}});label(clockText(position),x,y+16,18,muted);label(duration>0?clockText(duration):current_.duration,x+width-64,y+16,18,muted,64);}
    int lyricHeight(const std::string& value,int size,int width){auto key=std::to_string(size)+"|"+std::to_string(width)+"|"+value;auto found=lyricHeights_.find(key);if(found!=lyricHeights_.end())return found->second;auto* s=TTF_RenderUTF8_Blended_Wrapped(fonts_.at(size),value.c_str(),ink,width);int h=s?s->h:32;if(s)SDL_FreeSurface(s);lyricHeights_[key]=h;return h;}
    int lyricSize(const std::string& value){if(lyricHeight(value,28,580)<=120)return 28;if(lyricHeight(value,22,580)<=160)return 22;return 18;}
    void lyricText(const std::string& value,int x,int y,int size,SDL_Color fg,int width){if(value.empty())return;auto* s=TTF_RenderUTF8_Blended_Wrapped(fonts_.at(size),value.c_str(),fg,width);if(!s)return;auto* texture=SDL_CreateTextureFromSurface(renderer_,s);double scale=std::min(1.0,160.0/s->h);SDL_Rect dest{x,y,int(s->w*scale),int(s->h*scale)};SDL_RenderCopy(renderer_,texture,nullptr,&dest);SDL_DestroyTexture(texture);SDL_FreeSurface(s);}
    void textLink(const std::string& text,int x,int y,int size,int width,std::function<void()> action){
        if(text.empty())return;label(text,x,y,size,muted,width);int w=0,h=0;TTF_SizeUTF8(fonts_.at(size),text.c_str(),&w,&h);w=std::min(w,width);hits_.push_back({{x,y-5,w,std::max(44,h+10)},std::move(action)});
    }
    void audioIndicator(){
        Uint32 now=SDL_GetTicks();if(audioPoll_==0||now-audioPoll_>=750){audioPoll_=now;
#ifdef __SWITCH__
            if(!audioControlReady_)audioControlReady_=R_SUCCEEDED(audctlInitialize());
            AudioTarget target=AudioTarget_Invalid;if(audioControlReady_){Result rc=audctlGetActiveOutputTarget(&target);if(R_FAILED(rc))rc=audctlGetAudioOutputTargetForPlayReport(&target);audioRoute_=R_SUCCEEDED(rc)?audio_target_label(int(target)):"Audio: --";}
#endif
        }int width=0,height=0;TTF_SizeUTF8(fonts_.at(22),audioRoute_.c_str(),&width,&height);label(audioRoute_,1240-width,34,22,muted,240);
    }
    void drawFullscreen(){
        icon({30,16,64,64},4,[this]{fullscreen_=false;});label("Now playing",110,34,22,muted);
        cover(current_,{50,105,500,500});hits_.push_back({{50,105,500,500},[this]{fullscreen_=false;}});
        songTitle(current_.title,620,108,36,590);textLink(current_.artist,620,160,22,540,[this]{openArtist(current_);});textLink(current_.album.empty()?"View album":current_.album,620,208,18,540,[this]{openAlbum(current_);});
        const auto it=lyricCache_.find(current_.id);Lyrics lyrics=it==lyricCache_.end()?Lyrics{{},{},"Loading lyrics..."}:it->second;
        if(lyricScrollSong_!=current_.id){lyricScrollSong_=current_.id;lyricScroll_.reset();}lyricScrollMax_=0;
        SDL_Rect clip{620,260,600,270};SDL_RenderSetClipRect(renderer_,&clip);
        if(!lyrics.lines.empty()){
            int active=lyric_index(lyrics,player_->position());Uint32 now=SDL_GetTicks();double progress=lyric_ease((now-lyricAnimationStart_)/320.0);lyricVisual_=lyricFrom_+(lyricTarget_-lyricFrom_)*progress;
            if(lyricAnimationSong_!=current_.id||std::abs(active-lyricTarget_)>3){lyricAnimationSong_=current_.id;lyricTarget_=active;lyricFrom_=lyricVisual_=active;lyricAnimationStart_=now;}
            else if(active!=lyricTarget_){lyricFrom_=lyricVisual_;lyricTarget_=active;lyricAnimationStart_=now;}
            std::vector<double> centers;std::vector<int> heights,sizes;std::vector<std::string> lines{"..."};for(const auto& line:lyrics.lines)lines.push_back(lyric_display(line.text));
            for(size_t i=0;i<lines.size();i++){int size=lyricSize(lines[i]),height=std::min(160,lyricHeight(lines[i],size,580));sizes.push_back(size);heights.push_back(height);centers.push_back(i==0?0:centers.back()+heights[i-1]/2.0+24+height/2.0);}
            double index=std::clamp(lyricVisual_+1,0.0,double(centers.size()-1));size_t lower=size_t(index),upper=std::min(lower+1,centers.size()-1);double anchor=centers[lower]+(centers[upper]-centers[lower])*(index-lower);
            for(int i=std::max(-1,int(std::floor(lyricVisual_))-2);i<=std::min(int(lyrics.lines.size())-1,int(std::ceil(lyricVisual_))+2);i++){
                double distance=std::abs(i-lyricVisual_),weight=std::max(0.0,1-distance);SDL_Color fg{Uint8(muted.r+(ink.r-muted.r)*weight),Uint8(muted.g+(ink.g-muted.g)*weight),Uint8(muted.b+(ink.b-muted.b)*weight),Uint8(255*std::max(.2,1-distance*.35))};
                int top=int(400+centers[i+1]-anchor-heights[i+1]/2.0);if(top+heights[i+1]>clip.y&&top<clip.y+clip.h)lyricText(lines[i+1],620,top,sizes[i+1],fg,580);
            }
        }
        else if(!lyrics.plain.empty()){
            int total=0;for(const auto& text:lyrics.plain){auto line=lyric_display(text);total+=std::min(160,lyricHeight(line,lyricSize(line),580))+24;}
            lyricScrollMax_=std::max(0,total-24-clip.h);lyricScroll_.offset=std::clamp(lyricScroll_.offset,0.f,lyricScrollMax_);
            int top=clip.y-int(lyricScroll_.offset);for(const auto& text:lyrics.plain){auto line=lyric_display(text);int size=lyricSize(line),height=std::min(160,lyricHeight(line,size,580));if(top+height>clip.y&&top<clip.y+clip.h)lyricText(line,620,top,size,ink,580);top+=height+24;}
        }
        else {double pulse=.55+.25*std::sin(SDL_GetTicks()/650.0);SDL_Color fg=muted;fg.a=Uint8(255*pulse);lyricText("...",620,365,36,fg,580);hits_.push_back({clip,[this]{requestLyrics(current_,true);}});}
        SDL_RenderSetClipRect(renderer_,nullptr);timeline(620,555,580);transport(910,600);
    }
    void draw(){
        hits_.clear();rect({0,0,1280,720},paper);auto background=backgrounds_.find(current_.id);if(background!=backgrounds_.end()&&background->second){Uint8 tint=fullscreen_?255:194;SDL_SetTextureColorMod(background->second,tint,tint,tint);SDL_RenderCopy(renderer_,background->second,nullptr,nullptr);}audioIndicator();if(fullscreen_){if(!textures_.count(current_.id)&&SDL_GetTicks()-artworkRetry_>5000){artworkRetry_=SDL_GetTicks();artwork({current_});}drawFullscreen();SDL_RenderPresent(renderer_);return;}
        label("EchoNX",40,24,36,ink);label("YouTube Music",205,36,18,muted);
        cover(current_,{40,105,350,350});hits_.push_back({{40,105,350,350},[this]{focusPlayer();}});
        songTitle(current_.id.empty()?"Your next listen":current_.title,40,474,28,350);label(current_.id.empty()?"Pick a song on the right":current_.artist,40,512,22,muted,350);
        timeline(40,558,350);transport(215,608);
        label(tab_==2?"Your library":tab_==3?"Up next":tab_==4?artistAnchor_.artist:tab_==5?(albumAnchor_.album.empty()?"Album":albumAnchor_.album):tab_==0?"For you":heading_,440,106,36,ink,600);
        button({1050,104,190,54},tab_==1?"Search":tab_==2?"Search songs":tab_==3?"Discover":"Refresh",[this]{if(tab_==1||tab_==2)search();else if(tab_==5)fetch(4);else if(tab_==4)fetch(3);else if(tab_==0)fetch(0);else changeTab(0);});
        label(tab_==0?(library_.empty()?"YouTube Music discovery / save songs to tune this feed":"Recommendations based on your saved songs"):tab_==1?"Songs and artists":tab_==2?"Press - to save the playing song":tab_==4?"Songs from this artist's YouTube Music page":tab_==5?"Tracks from this album":"Songs play in this order",440,158,18,muted,790);
        auto& rows=visible();listScroll_.offset=std::clamp(listScroll_.offset,0.f,std::max(0.f,float(rows.size()*72)-390));
        SDL_Rect clip{440,199,800,390};SDL_RenderSetClipRect(renderer_,&clip);int start=int(listScroll_.offset)/72;size_t firstHit=hits_.size();
        if(start!=artworkStart_||tab_!=artworkTab_||SDL_GetTicks()-artworkRetry_>5000){artworkRetry_=SDL_GetTicks();artworkStart_=start;artworkTab_=tab_;std::vector<Song> shown;if(!current_.id.empty())shown.push_back(current_);for(int i=start;i<int(rows.size())&&i<start+7;i++)shown.push_back(rows[i]);artwork(shown);}
        for(int i=start;i<int(rows.size())&&i<start+7;i++){
            int y=199+i*72-int(listScroll_.offset);SDL_Rect bounds{440,y,800,66};round(bounds,14,i==selected_?surface:paper);
            if(i==selected_){color(accent);SDL_RenderDrawRect(renderer_,&bounds);}
            cover(rows[i],{450,y+8,50,50});songTitle(rows[i].title,515,y+6,22,480);label(rows[i].artist,515,y+37,18,muted,420);label(rows[i].duration,947,y+37,18,muted,65);
            auto song=rows[i];button({1020,y+9,100,48},"Artist",[this,song]{openArtist(song);},false,18);button({1130,y+9,105,48},"Album",[this,song]{openAlbum(song);},false,18);
            hits_.push_back({bounds,[this,i]{selected_=i;choose(i);}});
        }
        for(size_t i=firstHit;i<hits_.size();i++){SDL_Rect clipped{};SDL_IntersectRect(&hits_[i].rect,&clip,&clipped);hits_[i].rect=clipped;}
        SDL_RenderSetClipRect(renderer_,nullptr);if(rows.size()*72>390){int height=std::max(20,390*390/int(rows.size()*72));rect({1248,199+int((390-height)*listScroll_.offset/(rows.size()*72-390)),3,height},muted);}
        if(rows.empty())label(loading_?"Loading songs from YouTube Music...":tab_==2?"Your library is empty. Play a song, then press -.":tab_==3?"Choose a song to start an audio queue.":"Press X to search or tap Refresh.",450,255,22,muted,760);
        std::string message=loading_?"Loading YouTube Music...":notice_.empty()?player_->status():notice_;if(message!="Playing audio only")label(message,440,598,18,muted,780);label("Tap cover: Fullscreen   - Save   + Screen off",440,627,18,muted,800);
        for(int i=0;i<6;i++){SDL_Rect r{440+i*133,662,125,46};round(r,16,i==tab_?ink:surface);centeredLabel(tabNames[i],r,22,i==tab_?paper:ink);hits_.push_back({r,[this,i]{changeTab(i);}});}
        SDL_RenderPresent(renderer_);
    }
    void screenshot(const char* path){SDL_Surface* s=SDL_CreateRGBSurfaceWithFormat(0,1280,720,32,SDL_PIXELFORMAT_RGBA32);SDL_RenderReadPixels(renderer_,nullptr,s->format->format,s->pixels,s->pitch);IMG_SavePNG(s,path);SDL_FreeSurface(s);}
    void preview(){applyPending();applyPending();draw();screenshot("../../outputs/EchoNX-home-preview.png");if(!results_.empty()){current_=results_.front();queue_=results_;artwork(queue_);applyPending();draw();screenshot("../../outputs/EchoNX-player-preview.png");}changeTab(2);draw();screenshot("../../outputs/EchoNX-library-preview.png");search();applyPending();applyPending();draw();screenshot("../../outputs/EchoNX-search-preview.png");if(!searchRows_.empty()){current_=searchRows_.front();openArtist(current_);applyPending();applyPending();draw();screenshot("../../outputs/EchoNX-artist-preview.png");focusPlayer();draw();screenshot("../../outputs/EchoNX-fullscreen-preview.png");openAlbum(current_);applyPending();applyPending();draw();screenshot("../../outputs/EchoNX-album-preview.png");}}
#ifdef ECHO_PREVIEW
    void testPlainLyrics(){current_={"plain-test","Touchscreen lyric test","Test artist"};fullscreen_=true;Lyrics lyrics;for(int i=0;i<30;i++)lyrics.plain.push_back("Test lyric line "+std::to_string(i+1));lyricCache_[current_.id]=lyrics;draw();}
    void testFocusPlayer(){focusPlayer();}
    void testArtworkCache(){for(int i=0;i<80;i++){std::string id="cache-test-"+std::to_string(i);textures_[id]=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_STATIC,8,8);}applyPending();if(textures_.size()>32)throw std::runtime_error("Artwork cache exceeded limit");}
    float testLyricScroll()const{return lyricScroll_.offset;}
    void testLongLyrics(){std::string longLine;for(int i=0;i<14;i++)longLine+="A longer lyric line for layout testing. ";lyricCache_[current_.id]={{{0,longLine},{5,"Short following line"}}, {},"Synthetic layout test"};lyricAnimationSong_.clear();draw();screenshot("../../outputs/EchoNX-long-lyrics-preview.png");}
    Json testState()const{return {{"song",current_.id},{"fullscreen",fullscreen_},{"tab",tab_},{"scroll",listScroll_.offset}};}
#endif
    void run(){Uint32 previousFrame=SDL_GetTicks();while(running_){Uint32 now=SDL_GetTicks();float elapsed=std::min(.05f,(now-previousFrame)/1000.f);previousFrame=now;
#ifdef __SWITCH__
        if(!appletMainLoop())break;padUpdate(&pad_);uint64_t down=padGetButtonsDown(&pad_);
            if(!fullscreen_&&!screenOff_&&!pointerActive_){auto left=padGetStickPos(&pad_,0),right=padGetStickPos(&pad_,1);float speed=analog_scroll_speed(left.y,right.y);int direction=std::abs(speed)>90?(speed>0?1:-1):0;if(direction!=analogDirection_){analogDirection_=direction;if(direction){action(direction>0?5:4);analogRepeat_=now+350;}}else if(direction&&int32_t(now-analogRepeat_)>=0){action(direction>0?5:4);analogRepeat_=now+140;}}else analogDirection_=0;
        const uint64_t masks[]={HidNpadButton_A,HidNpadButton_B,HidNpadButton_X,HidNpadButton_Y,HidNpadButton_Up,HidNpadButton_Down,HidNpadButton_L,HidNpadButton_R,HidNpadButton_ZL,HidNpadButton_ZR,HidNpadButton_Plus,HidNpadButton_Minus,HidNpadButton_StickR,HidNpadButton_StickL};
        for(int i=0;i<14;i++)if(down&masks[i]){action(i);break;}
#endif
        SDL_Event event;while(SDL_PollEvent(&event)){
            if(event.type==SDL_QUIT)running_=false;
            if(event.type==SDL_WINDOWEVENT && event.window.event==SDL_WINDOWEVENT_FOCUS_LOST)wake();
            if(event.type==SDL_MOUSEBUTTONDOWN||event.type==SDL_FINGERDOWN){if(pointerActive_)continue;if(screenOff_){wake();wakeTouch_=true;continue;}pointerX_=event.type==SDL_FINGERDOWN?int(event.tfinger.x*1280):event.button.x;pointerY_=event.type==SDL_FINGERDOWN?int(event.tfinger.y*720):event.button.y;pointerFinger_=event.type==SDL_FINGERDOWN?event.tfinger.fingerId:0;pointerActive_=true;pointerOnList_=!fullscreen_&&pointerX_>=440&&pointerX_<1240&&pointerY_>=199&&pointerY_<589;pointerOnLyrics_=fullscreen_&&lyricScrollMax_>0&&pointerX_>=620&&pointerX_<1220&&pointerY_>=260&&pointerY_<530;if(pointerOnList_)listScroll_.begin(pointerY_);if(pointerOnLyrics_)lyricScroll_.begin(pointerY_);}
            if(event.type==SDL_MOUSEMOTION||event.type==SDL_FINGERMOTION){if(!pointerActive_)continue;if(event.type==SDL_FINGERMOTION&&event.tfinger.fingerId!=pointerFinger_)continue;pointerX_=event.type==SDL_FINGERMOTION?int(event.tfinger.x*1280):event.motion.x;pointerY_=event.type==SDL_FINGERMOTION?int(event.tfinger.y*720):event.motion.y;if(pointerOnList_)listScroll_.move(pointerY_,std::max(0.f,float(visible().size()*72)-390));if(pointerOnLyrics_)lyricScroll_.move(pointerY_,lyricScrollMax_);}
            if(event.type==SDL_MOUSEBUTTONUP||event.type==SDL_FINGERUP){if(wakeTouch_){wakeTouch_=false;continue;}if(!pointerActive_)continue;if(event.type==SDL_FINGERUP&&event.tfinger.fingerId!=pointerFinger_)continue;pointerX_=event.type==SDL_FINGERUP?int(event.tfinger.x*1280):event.button.x;pointerY_=event.type==SDL_FINGERUP?int(event.tfinger.y*720):event.button.y;bool tap=true;if(pointerOnList_){listScroll_.move(pointerY_,std::max(0.f,float(visible().size()*72)-390));tap=listScroll_.end();}if(pointerOnLyrics_){lyricScroll_.move(pointerY_,lyricScrollMax_);tap=lyricScroll_.end();}pointerActive_=false;if(tap)for(auto& hit:hits_)if(pointerX_>=hit.rect.x&&pointerX_<hit.rect.x+hit.rect.w&&pointerY_>=hit.rect.y&&pointerY_<hit.rect.y+hit.rect.h){auto action=hit.action;action();break;}}
            if(event.type==SDL_MOUSEWHEEL&&!fullscreen_)listScroll_.offset=std::clamp(listScroll_.offset-event.wheel.y*48.f,0.f,std::max(0.f,float(visible().size()*72)-390));
            if(event.type==SDL_KEYDOWN){switch(event.key.keysym.sym){case SDLK_RETURN:action(0);break;case SDLK_ESCAPE:action(12);break;case SDLK_UP:action(4);break;case SDLK_DOWN:action(5);break;case SDLK_x:action(2);break;case SDLK_SPACE:action(3);break;case SDLK_LEFT:action(6);break;case SDLK_RIGHT:action(7);break;}}
        }
        applyPending();if(player_->ended())next(1);
#ifdef __SWITCH__
        appletSetMediaPlaybackState(player_->active());
#endif
        if(screenOff_){if(!player_->active())wake();SDL_Delay(20);}else{draw();SDL_Delay(16);}
    }}
};
}
int main(int argc,char** argv){
    try{
#ifndef ECHO_PREVIEW
        curl_global_init(CURL_GLOBAL_DEFAULT);
#endif
        {echo::App app;if(argc>1 && std::string(argv[1])=="--preview")app.preview();else app.run();}
#ifndef ECHO_PREVIEW
        curl_global_cleanup();
#endif
        return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"EchoNX: %s\n",e.what());return 1;}
}
