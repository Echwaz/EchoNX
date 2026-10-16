    void rebuildHistory(){historyRows_.clear();for(const auto& entry:history_)historyRows_.push_back(entry.song);}
    void showHistory(){historyView_=true;playlistsView_=false;activePlaylist_=-1;tab_=2;fullscreen_=false;selected_=0;listScroll_.reset();artwork(historyRows_);}
    void showSavedSongs(){tab_=2;fullscreen_=false;historyView_=playlistsView_=false;activePlaylist_=-1;selected_=0;listScroll_.reset();artwork(library_);}
    void collectHistory(){
        if(historyCandidate_.empty()||current_.id!=historyCandidate_||!player_->hasStarted())return;historyCandidate_.clear();
        int previous=selected_;std::string selectedId=historyView_&&tab_==2&&selected_>=0&&selected_<int(historyRows_.size())?historyRows_[selected_].id:"";
        record_listen(history_,current_,std::time(nullptr));rebuildHistory();
        if(!selectedId.empty()){auto it=std::find_if(historyRows_.begin(),historyRows_.end(),[&](const Song& song){return song.id==selectedId;});if(it!=historyRows_.end()){selected_=int(it-historyRows_.begin());listScroll_.offset=std::clamp(listScroll_.offset+(selected_-previous)*72.f,0.f,std::max(0.f,float(historyRows_.size()*72)-390));}}
        if(historyWriter_)historyWriter_->submit(history_document(history_));
    }
    bool playlistBrowser()const{return tab_==2&&playlistsView_&&activePlaylist_<0;}
    int rowCount(){return playlistBrowser()?int(playlists_.size()):int(visible().size());}
    int findPlaylist(const std::string& id)const{for(int i=0;i<int(playlists_.size());++i)if(playlists_[i].id==id)return i;return -1;}
    std::string libraryHeading()const{return historyView_?"Recently played":!playlistsView_?"Your library":activePlaylist_>=0&&activePlaylist_<int(playlists_.size())?playlists_[activePlaylist_].name:"Your playlists";}
    void showPlaylistRoot(){historyView_=false;playlistsView_=true;activePlaylist_=-1;tab_=2;fullscreen_=false;selected_=0;listScroll_.reset();}
    void toggleLibraryView(){historyView_=false;if(playlistsView_&&activePlaylist_>=0){showPlaylistRoot();return;}playlistsView_=!playlistsView_;activePlaylist_=-1;selected_=0;listScroll_.reset();}
    void openPlaylist(int index){if(index<0||index>=int(playlists_.size()))return;historyView_=false;playlistsView_=true;activePlaylist_=index;tab_=2;selected_=0;listScroll_.reset();artwork(playlists_[index].tracks);}
    std::string enterName(const std::string& heading,const std::string& initial=""){
#ifdef __SWITCH__
        SwkbdConfig keyboard{};if(R_FAILED(swkbdCreate(&keyboard,0)))return {};swkbdConfigMakePresetDefault(&keyboard);swkbdConfigSetHeaderText(&keyboard,heading.c_str());swkbdConfigSetInitialText(&keyboard,initial.c_str());swkbdConfigSetStringLenMax(&keyboard,60);char buffer[256]{};Result rc=swkbdShow(&keyboard,buffer,sizeof(buffer));swkbdClose(&keyboard);if(R_FAILED(rc))return {};std::string name=buffer;
#else
        std::string name=initial.empty()?"New playlist":initial+" renamed";
#endif
        auto first=name.find_first_not_of(" \t\r\n"),last=name.find_last_not_of(" \t\r\n");return first==std::string::npos?"":name.substr(first,last-first+1);
    }
    bool commitPlaylists(std::vector<LocalPlaylist> previous){try{save_document(playlistsPath_,playlists_document(playlists_));return true;}catch(const std::exception& e){playlists_=std::move(previous);notice_=e.what();return false;}}
    void newPlaylist(Song song={}){
        auto name=enterName("Playlist name");if(name.empty())return;auto previous=playlists_;std::string id=std::to_string(std::time(nullptr))+"-"+std::to_string(SDL_GetPerformanceCounter());while(findPlaylist(id)>=0)id+="x";
        LocalPlaylist list{id,name,{}};if(!song.id.empty())playlist_add(list,song);playlists_.push_back(std::move(list));if(commitPlaylists(std::move(previous)))notice_="Created playlist: "+name;
    }
    void addToPlaylist(const std::string& id,const Song& song){int index=findPlaylist(id);if(index<0)return;auto previous=playlists_;if(!playlist_add(playlists_[index],song)){notice_="This song is already in the playlist.";return;}auto name=playlists_[index].name;if(commitPlaylists(std::move(previous)))notice_="Added to "+name;}
    void closeMenu(){hits_.clear();menu_.clear();menuSelection_=menuFirst_=0;menuTitle_.clear();}
    void openMenu(const std::string& title,std::vector<MenuEntry> entries){hits_.clear();menuTitle_=title;menu_=std::move(entries);menuSelection_=menuFirst_=0;pointerActive_=false;pointerOnSeek_=false;seekBar_.cancel();listScroll_.end();}
    void activateMenu(int index){if(index<0||index>=int(menu_.size()))return;auto action=menu_[index].action;closeMenu();action();}
    void playlistPicker(Song song){std::vector<MenuEntry> options;options.push_back({"New playlist...",[this,song]{newPlaylist(song);}});for(const auto& list:playlists_){auto id=list.id;options.push_back({list.name,[this,id,song]{addToPlaylist(id,song);}});}options.push_back({"Cancel",[this]{}});openMenu("Add to playlist",std::move(options));}
    void playlistOptions(std::string id){int index=findPlaylist(id);if(index<0)return;auto title=playlists_[index].name;
        openMenu(title,{{"New playlist...",[this]{newPlaylist();}},{"Play playlist",[this,id]{int i=findPlaylist(id);if(i<0)return;openPlaylist(i);if(!playlists_[i].tracks.empty())choose(0);else notice_="This playlist is empty.";}},
            {"Rename playlist",[this,id]{int i=findPlaylist(id);if(i<0)return;auto name=enterName("Rename playlist",playlists_[i].name);if(name.empty())return;auto previous=playlists_;playlists_[i].name=name;commitPlaylists(std::move(previous));}},
            {"Delete playlist",[this,id,title]{openMenu("Delete "+title+"?",{{"Delete playlist",[this,id]{int i=findPlaylist(id);if(i<0)return;auto previous=playlists_;playlists_.erase(playlists_.begin()+i);if(commitPlaylists(std::move(previous)))showPlaylistRoot();}},{"Cancel",[this]{}}});}},
            {"Cancel",[this]{}}});
    }
    void songOptions(){
        if(playlistBrowser()){if(selected_<int(playlists_.size()))playlistOptions(playlists_[selected_].id);else newPlaylist();return;}
        auto& rows=visible();Song song=fullscreen_?current_:selected_<int(rows.size())?rows[selected_]:current_;if(song.id.empty()){if(tab_==2){std::vector<MenuEntry> entries{{"Your playlists",[this]{showPlaylistRoot();}},{"New playlist...",[this]{newPlaylist();}},{"Recently played",[this]{showHistory();}},{"Search songs",[this]{search();}},{"Cancel",[this]{}}};if(historyView_)entries.insert(entries.begin(),{"Library",[this]{showSavedSongs();}});openMenu("Your library",std::move(entries));}else notice_="Choose a song first.";return;}
        std::vector<MenuEntry> options{{"Add to playlist",[this,song]{playlistPicker(song);}}};
        if(tab_==2&&playlistsView_&&activePlaylist_>=0){auto id=playlists_[activePlaylist_].id;options.push_back({"Remove from this playlist",[this,id,song]{int i=findPlaylist(id);if(i<0)return;auto previous=playlists_;auto& tracks=playlists_[i].tracks;tracks.erase(std::remove_if(tracks.begin(),tracks.end(),[&](const Song& s){return s.id==song.id;}),tracks.end());if(commitPlaylists(std::move(previous))){selected_=std::min(selected_,std::max(0,int(tracks.size())-1));notice_="Removed from playlist.";}}});}
        if(tab_==2&&!playlistsView_)options.push_back({"Your playlists",[this]{showPlaylistRoot();}});
        if(tab_==2&&!historyView_)options.push_back({"Recently played",[this]{showHistory();}});
        if(tab_==2&&historyView_)options.push_back({"Library",[this]{showSavedSongs();}});
        options.push_back({"Cancel",[this]{}});openMenu(song.title,std::move(options));
    }
    void drawMenu(){
        if(menu_.empty())return;rect({0,0,1280,720},{0,0,0,185});round({325,105,630,510},22,paper);label(menuTitle_,355,128,28,ink,570);hits_.clear();
        menuFirst_=std::clamp(menuFirst_,0,std::max(0,int(menu_.size())-6));if(menuSelection_<menuFirst_)menuFirst_=menuSelection_;if(menuSelection_>=menuFirst_+6)menuFirst_=menuSelection_-5;
        for(int i=menuFirst_;i<int(menu_.size())&&i<menuFirst_+6;++i){SDL_Rect bounds{350,188+(i-menuFirst_)*62,580,54};round(bounds,12,i==menuSelection_?ink:surface);label(menu_[i].text,bounds.x+16,bounds.y+12,22,i==menuSelection_?paper:ink,548);hits_.push_back({bounds,[this,i]{activateMenu(i);}});}
        label("A Select   B Back",355,580,18,muted,300);if(menuFirst_>0)button({710,570,100,34},"Previous",[this]{menuSelection_=std::max(0,menuSelection_-6);},false,18);if(menuFirst_+6<int(menu_.size()))button({820,570,110,34},"Next",[this]{menuSelection_=std::min(int(menu_.size())-1,menuSelection_+6);},false,18);
    }
    void menuAction(int key){if(key==0){activateMenu(menuSelection_);return;}if(key==1||key==2||key==12){closeMenu();return;}if(key==4)menuSelection_=std::max(0,menuSelection_-1);if(key==5)menuSelection_=std::min(int(menu_.size())-1,menuSelection_+1);}
    void checkpoint(bool force=false){
        if(!sessionWriter_)return;Uint32 now=SDL_GetTicks();if(!force&&now-sessionSaved_<10000)return;sessionSaved_=now;
        SessionSnapshot session{queue_,playbackOrder_.displayOrder(),queueIndex_,playbackOrder_.repeat,playbackOrder_.shuffle,resumePending_?resumePosition_:player_->position(),resumePending_?resumeDuration_:player_->duration()};
        if(session.queue.empty())return;sessionWriter_->submit(session_document(session));
    }
    void startCurrent(double position=0){if(current_.id.empty())return;resumePending_=false;resumePosition_=0;player_->play(current_,position);historyCandidate_=current_.id;checkpoint(true);}
