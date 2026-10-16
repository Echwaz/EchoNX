    static int resultTab(int mode){return mode==0?0:mode==1?1:mode==3?4:mode==4?5:-1;}
    void fetch(int mode,const std::string& value="",bool fresh=false){
        FetchJob job;job.mode=mode;job.searchKind=searchKind_;job.value=value;job.fresh=fresh;job.anchor=mode==4?albumAnchor_:artistAnchor_;job.playing=current_;if(mode==0)job.saved=library_;job.offset=homeOffset_;if(mode==0)++homeOffset_;
        {std::lock_guard<std::mutex> lock(dataMutex_);job.revision=++fetchRevisions_[mode];fetchQueue_.erase(std::remove_if(fetchQueue_.begin(),fetchQueue_.end(),[&](const FetchJob& old){return old.mode==mode;}),fetchQueue_.end());if(fetchCancel_&&(fetchActiveMode_==mode||mode!=2))fetchCancel_->store(true);loading_=true;
#ifndef ECHO_PREVIEW
            if(mode==2)fetchQueue_.push_back(job);else fetchQueue_.push_front(job);
#endif
        }
#ifdef ECHO_PREVIEW
        FetchResult result;result.job=job;const char* path=mode==4?"tests/album.json":mode==3?"tests/artist-songs.json":mode==2?"tests/related.json":mode==1?"tests/search.json":"tests/home.json";
        std::ifstream input(path);Json root;input>>root;result.rows=mode==1?search_results(root,job.searchKind):songs(root,mode>=3?120:40);if(mode==0&&!job.saved.empty()){std::ifstream related("tests/related.json");Json rows;related>>rows;result.rows=rank_recommendations(job.saved,{songs(rows)});}pendingFetch_.push_back(std::move(result));loading_=false;
#else
        if(!fetch_.joinable())fetch_=std::thread([this]{
            prefer_cpu(0);for(;;){FetchJob job;std::shared_ptr<std::atomic<bool>> cancel;
                {std::unique_lock<std::mutex> lock(dataMutex_);fetchCv_.wait(lock,[this]{return closing_||!fetchQueue_.empty();});if(closing_)break;job=std::move(fetchQueue_.front());fetchQueue_.pop_front();fetchActiveMode_=job.mode;fetchCancel_=cancel=std::make_shared<std::atomic<bool>>(false);}
                FetchResult result;result.job=job;
                try{
                    result.rows=job.mode==4?api_.album(job.anchor,job.fresh,cancel.get()):job.mode==3?api_.artist(job.anchor,job.fresh,cancel.get()):job.mode==2?api_.related(job.value,job.fresh,cancel.get()):job.mode==1?api_.search(job.value,job.fresh,cancel.get(),job.searchKind):api_.for_saved(job.saved,job.offset,job.fresh,cancel.get());
                    if(job.mode==2){for(const auto& song:result.rows)if(song.id==job.value)result.metadata=song;
                        if(!cancel->load()&&job.playing.id==job.value&&job.playing.album.empty()&&result.metadata.album.empty())try{for(const auto& song:api_.search(job.playing.title+" "+job.playing.artist,false,cancel.get()))if(song.id==job.value){result.metadata=song;break;}}catch(...){ }
                        result.rows.erase(std::remove_if(result.rows.begin(),result.rows.end(),[&](const Song& song){return song.id==job.value;}),result.rows.end());}
                    if(result.rows.empty())result.notice="No songs returned. Open Search or retry Refresh.";
                }catch(const std::exception& e){result.notice=std::string(e.what())+"  Tap Refresh to retry.";}
                {std::lock_guard<std::mutex> lock(dataMutex_);if(!closing_&&job.revision==fetchRevisions_[job.mode]){if(!cancel->load())pendingFetch_.push_back(std::move(result));else fetchQueue_.push_back(std::move(job));}fetchActiveMode_=-1;fetchCancel_.reset();loading_=!fetchQueue_.empty();}
            }
        });fetchCv_.notify_one();
#endif
    }
    void applyFetchResults(){
        while(!pendingFetch_.empty()){
            auto result=std::move(pendingFetch_.front());pendingFetch_.pop_front();const auto& job=result.job;
            if(job.revision!=fetchRevisions_[job.mode])continue;
            if(job.mode==1&&(job.value!=query_||job.searchKind!=searchKind_))continue;
            if(job.mode==2&&job.value!=current_.id)continue;
            if((job.mode==3||job.mode==4)&&pageKey(job.mode,job.anchor)!=pageKey(job.mode,job.mode==4?albumAnchor_:artistAnchor_))continue;
            auto& target=job.mode==4?albumRows_:job.mode==3?artistRows_:job.mode==2?relatedRows_:job.mode==1?searchRows_:results_;target=std::move(result.rows);
            if(job.mode==2){relatedFor_=job.value;if(!result.metadata.id.empty())pendingMetadata_=std::move(result.metadata);}
            if(tab_==resultTab(job.mode)){selected_=0;listScroll_.reset();notice_=result.notice;artworkStart_=-1;artworkTab_=-1;}
        }
    }
