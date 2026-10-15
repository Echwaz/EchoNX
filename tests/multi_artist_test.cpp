#include "model.hpp"
#include <cassert>
#include <iostream>
int main(){
    auto artist=[](const char* name,const char* id,const char* type){echo::Json run;run["text"]=name;run["navigationEndpoint"]["browseEndpoint"]["browseId"]=id;run["navigationEndpoint"]["browseEndpoint"]["browseEndpointContextSupportedConfigs"]["browseEndpointContextMusicConfig"]["pageType"]=type;return run;};
    auto a=artist("Artist A","UC-a","MUSIC_PAGE_TYPE_ARTIST"),b=artist("Artist B","UC-b","MUSIC_PAGE_TYPE_ARTIST"),album=artist("Album title","MPRE-album","MUSIC_PAGE_TYPE_ALBUM");
    for(const char* key:{"musicResponsiveListItemRenderer","musicTwoRowItemRenderer","playlistPanelVideoRenderer"}){echo::Json row;row["videoId"]="track";row["title"]["simpleText"]="Track";row["longBylineText"]["runs"]=echo::Json::array({a,{{"text"," & "}},b,album});row["subtitle"]["runs"]=echo::Json::array({a,b});auto songs=echo::songs(echo::Json{{key,row}});assert(songs.size()==1);auto song=songs.front();assert(song.artist=="Artist A"&&song.artistId=="UC-a");assert(song.artistNames.size()==2&&echo::display_artists(song)=="Artist A & Artist B");assert(echo::display_artists(echo::from_json(echo::to_json(song)))=="Artist A & Artist B");}
    auto old=echo::from_json({{"id","old"},{"artist","Legacy artist"}});assert(echo::display_artists(old)=="Legacy artist");assert(echo::display_artists(echo::from_json({{"artists",nullptr}})).empty());
    std::cout<<"PASS: multi-artist parsing in all renderers, duplicate suppression, album exclusion, primary identity, saved metadata and legacy library.\n";
}