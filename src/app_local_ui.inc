    int librarySection()const{return historyView_?2:playlistsView_?1:0;}
    void selectLibrarySection(int section){
        if(section==librarySection())return;
        if(section==0)showSavedSongs();else if(section==1)showPlaylistRoot();else showHistory();
        artworkStart_=artworkTab_=-1;hits_.clear();
    }
    void stepLibrarySection(int direction){selectLibrarySection((librarySection()+direction+3)%3);}
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
    bool commitPlaylists(){
        if(!playlistWriter_){notice_="Playlist storage unavailable.";return false;}playlistTicket_=playlistWriter_->submit(playlists_document(playlists_));playlistRetry_=false;return true;
    }
    void pollPlaylistStorage(){
        if(!playlistWriter_)return;auto error=playlistWriter_->takeError();if(!error.empty()){notice_="Playlist changes not saved: "+error+" Retrying...";playlistRetry_=true;playlistRetryAt_=SDL_GetTicks();}
        if(playlistWriter_->completed()>=playlistTicket_)playlistRetry_=false;
        if(playlistRetry_&&SDL_GetTicks()-playlistRetryAt_>=2000){playlistRetryAt_=SDL_GetTicks();playlistTicket_=playlistWriter_->submit(playlists_document(playlists_));}
    }
    void newPlaylist(Song song={}){
        auto name=enterName("Playlist name");if(name.empty())return;std::string id=std::to_string(std::time(nullptr))+"-"+std::to_string(SDL_GetPerformanceCounter());while(findPlaylist(id)>=0)id+="x";
        LocalPlaylist list{id,name,{}};if(!song.id.empty())playlist_add(list,song);playlists_.push_back(std::move(list));if(commitPlaylists())notice_="Created playlist: "+name;
    }
    void addToPlaylist(const std::string& id,const Song& song){int index=findPlaylist(id);if(index<0)return;if(!playlist_add(playlists_[index],song)){notice_="This song is already in the playlist.";return;}auto name=playlists_[index].name;if(commitPlaylists())notice_="Added to "+name;}
    void closeMenu(){hits_.clear();menu_.clear();menuSelection_=menuFirst_=0;menuTitle_.clear();}
    void openMenu(const std::string& title,std::vector<MenuEntry> entries){hits_.clear();menuTitle_=title;menu_=std::move(entries);menuSelection_=menuFirst_=0;pointerActive_=false;pointerOnSeek_=false;seekBar_.cancel();listScroll_.end();}
    void activateMenu(int index){if(index<0||index>=int(menu_.size()))return;auto action=menu_[index].action;closeMenu();action();}
    void playlistPicker(Song song){std::vector<MenuEntry> options;options.push_back({"New playlist...",[this,song]{newPlaylist(song);}});for(const auto& list:playlists_){auto id=list.id;options.push_back({list.name,[this,id,song]{addToPlaylist(id,song);}});}options.push_back({"Cancel",[this]{}});openMenu("Add to playlist",std::move(options));}
    void playlistOptions(std::string id){int index=findPlaylist(id);if(index<0)return;auto title=playlists_[index].name;
        openMenu(title,{{"New playlist...",[this]{newPlaylist();}},{"Play playlist",[this,id]{int i=findPlaylist(id);if(i<0)return;openPlaylist(i);if(!playlists_[i].tracks.empty())choose(0);else notice_="This playlist is empty.";}},
            {"Rename playlist",[this,id]{int i=findPlaylist(id);if(i<0)return;auto name=enterName("Rename playlist",playlists_[i].name);if(name.empty())return;playlists_[i].name=name;commitPlaylists();}},
            {"Delete playlist",[this,id,title]{openMenu("Delete "+title+"?",{{"Delete playlist",[this,id]{int i=findPlaylist(id);if(i<0)return;playlists_.erase(playlists_.begin()+i);if(commitPlaylists())showPlaylistRoot();}},{"Cancel",[this]{}}});}},
            {"Cancel",[this]{}}});
    }
    void songOptions(){
        if(!fullscreen_&&playlistBrowser()){if(selected_<int(playlists_.size()))playlistOptions(playlists_[selected_].id);else newPlaylist();return;}
        Song song;
        if(fullscreen_)song=current_;else{auto& rows=visible();if(selected_>=0&&selected_<int(rows.size()))song=rows[selected_];}
        if(song.id.empty()){if(!fullscreen_&&tab_==2)openMenu("Your library",{{"New playlist...",[this]{newPlaylist();}},{"Cancel",[this]{}}});else notice_="Choose a song first.";return;}
        if(tab_==1&&searchKind_!=0&&!fullscreen_){openMenu(song.title,{{searchKind_==1?"Open album":"Open artist",[this,song]{if(searchKind_==1)openAlbum(song);else openArtist(song);}},{"Cancel",[this]{}}});return;}
        std::vector<MenuEntry> options{{"Add to playlist",[this,song]{playlistPicker(song);}}};
        options.push_back({"Play next",[this,song]{playNextSong(song);}});
        bool saved=std::any_of(library_.begin(),library_.end(),[&](const Song& entry){return entry.id==song.id;});
        options.push_back({saved?"Remove from library":"Save to library",[this,song]{favorite(song);selected_=std::min(selected_,std::max(0,rowCount()-1));}});
        if(!fullscreen_&&tab_==2&&playlistsView_&&activePlaylist_>=0){auto id=playlists_[activePlaylist_].id;options.push_back({"Remove from this playlist",[this,id,song]{int i=findPlaylist(id);if(i<0)return;auto& tracks=playlists_[i].tracks;tracks.erase(std::remove_if(tracks.begin(),tracks.end(),[&](const Song& s){return s.id==song.id;}),tracks.end());if(commitPlaylists()){selected_=std::min(selected_,std::max(0,int(tracks.size())-1));notice_="Removed from playlist.";}}});}
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
    void startCurrent(double position=0){if(current_.id.empty())return;resumePending_=false;resumePosition_=0;if(relatedFor_!=current_.id)relatedRows_.clear();player_->play(current_,position);historyCandidate_=current_.id;checkpoint(true);}
