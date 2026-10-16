    static std::string artKey(const Song& song){return song.cover.empty()?"id|"+song.id:song.cover;}
    void publishArtwork(const Song& song,PreparedArt ready){
        {std::lock_guard<std::mutex> lock(dataMutex_);if(closing_)return;auto& pending=artPending_[song.id];if(ready.image||!pending.image)pending=std::move(ready);
            if(fullArtWanted_.count(song.id)&&!pending.image&&std::none_of(artCacheRequests_.begin(),artCacheRequests_.end(),[&](const Song& item){return item.id==song.id;}))artCacheRequests_.insert(artCacheRequests_.begin(),song);
        }artCacheCv_.notify_one();
    }
    bool reuseArtwork(const Song& song){
        bool full;{std::lock_guard<std::mutex> lock(dataMutex_);full=fullArtWanted_.count(song.id);}
        PreparedArt ready;{std::lock_guard<std::mutex> lock(imageDecodeMutex_);auto found=decodedArt_.find(artKey(song));if(found==decodedArt_.end()||!found->second.art.thumbnail||(full&&!found->second.art.image))return false;found->second.used=++decodedClock_;ready=found->second.art;}
        if(!full){ready.image.reset();ready.background.reset();ready.clock.reset();}
        publishArtwork(song,std::move(ready));return true;
    }
    void prepareArtwork(const Song& song,const std::string& bytes){
        bool full;std::set<std::string> protectedKeys;{std::lock_guard<std::mutex> lock(dataMutex_);full=fullArtWanted_.count(song.id);protectedKeys=fullArtKeys_;}
        PreparedArt ready;
        {std::lock_guard<std::mutex> lock(imageDecodeMutex_);auto key=artKey(song);auto found=decodedArt_.find(key);
            if(found!=decodedArt_.end()&&found->second.art.thumbnail&&(!full||found->second.art.image)){found->second.used=++decodedClock_;ready=found->second.art;}
            else{
                auto* image=IMG_Load_RW(SDL_RWFromConstMem(bytes.data(),int(bytes.size())),1);if(!image)return;ready.key=key;
                auto* thumbnail=SDL_CreateRGBSurfaceWithFormat(0,thumbnailSize,thumbnailSize,32,SDL_PIXELFORMAT_RGBA32);if(thumbnail){SDL_BlitScaled(image,nullptr,thumbnail,nullptr);ready.thumbnail={thumbnail,SDL_FreeSurface};}
                if(full){ready.image={image,SDL_FreeSurface};auto* background=album_blur_surface(image,.25f);if(background)ready.background={background,SDL_FreeSurface};auto* clock=clock_backdrop(image);if(clock)ready.clock={clock,SDL_FreeSurface};}else SDL_FreeSurface(image);
                decodedArt_[key]={ready,++decodedClock_};
                while(decodedArt_.size()>thumbnailLimit){auto victim=decodedArt_.end();for(auto it=decodedArt_.begin();it!=decodedArt_.end();++it)if(!protectedKeys.count(it->first)&&(victim==decodedArt_.end()||it->second.used<victim->second.used))victim=it;if(victim==decodedArt_.end())break;decodedArt_.erase(victim);}
                int fullCount=0;for(const auto& entry:decodedArt_)if(entry.second.art.image)++fullCount;
                while(fullCount>6){auto victim=decodedArt_.end();for(auto it=decodedArt_.begin();it!=decodedArt_.end();++it)if(it->second.art.image&&!protectedKeys.count(it->first)&&(victim==decodedArt_.end()||it->second.used<victim->second.used))victim=it;if(victim==decodedArt_.end())break;auto& art=victim->second.art;art.image.reset();art.background.reset();art.clock.reset();--fullCount;}
            }
        }
        if(!full){ready.image.reset();ready.background.reset();ready.clock.reset();}
        publishArtwork(song,std::move(ready));
    }
    TexturePtr artworkTexture(std::map<std::string,std::weak_ptr<SDL_Texture>>& pool,const std::string& key,SDL_Surface* image){
        if(auto existing=pool[key].lock())return existing;if(!image)return {};auto* texture=SDL_CreateTextureFromSurface(renderer_,image);if(!texture)return {};TexturePtr result(texture,SDL_DestroyTexture);pool[key]=result;
        if(pool.size()>256)for(auto it=pool.begin();it!=pool.end();)if(it->second.expired())it=pool.erase(it);else ++it;
        return result;
    }
