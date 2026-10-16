#pragma once
#include "model.hpp"
namespace echo {
inline const char* search_params(int kind){return kind==1?"EgWKAQIYAWoMEA4QChADEAQQCRAF":kind==2?"EgWKAQIgAWoMEA4QChADEAQQCRAF":"EgWKAQIIAWoKEAkQBRAKEAMQBA%3D%3D";}
inline std::vector<Song> search_results(const Json& root,int kind){
    if(kind==0)return songs(root);std::vector<Song> rows;std::unordered_set<std::string> seen;
    std::function<void(const Json&)> walk=[&](const Json& node){if(rows.size()>=40)return;if(node.is_array()){for(const auto& child:node)walk(child);return;}if(!node.is_object())return;
        for(const char* key:{"musicResponsiveListItemRenderer","musicTwoRowItemRenderer"})if(node.contains(key)){
            const auto& item=node.at(key);auto id=str(item,"/navigationEndpoint/browseEndpoint/browseId");if(id.empty())id=str(item,"/flexColumns/0/musicResponsiveListItemFlexColumnRenderer/text/runs/0/navigationEndpoint/browseEndpoint/browseId");
            auto type=str(item,"/navigationEndpoint/browseEndpoint/browseEndpointContextSupportedConfigs/browseEndpointContextMusicConfig/pageType");
            if(kind==1?(type!="MUSIC_PAGE_TYPE_ALBUM"&&id.rfind("MPRE",0)!=0):(type!="MUSIC_PAGE_TYPE_ARTIST"&&id.rfind("UC",0)!=0))return;
            if(id.empty()||!seen.insert(id).second)return;Song row;row.id=id;if(item.contains("title"))row.title=text(item["title"]);if(row.title.empty()&&item.contains(Json::json_pointer("/flexColumns/0/musicResponsiveListItemFlexColumnRenderer/text")))row.title=text(item.at(Json::json_pointer("/flexColumns/0/musicResponsiveListItemFlexColumnRenderer/text")));
            for(const char* path:{"/subtitle/runs","/flexColumns/1/musicResponsiveListItemFlexColumnRenderer/text/runs"}){Json::json_pointer pointer(path);if(!item.contains(pointer)||!item.at(pointer).is_array())continue;for(const auto& run:item.at(pointer)){auto artist=str(run,"/navigationEndpoint/browseEndpoint/browseId");if(artist.rfind("UC",0)==0){row.artist=str(run,"/text");row.artistId=artist;break;}}}
            for(const char* path:{"/thumbnail/musicThumbnailRenderer/thumbnail/thumbnails","/thumbnailRenderer/musicThumbnailRenderer/thumbnail/thumbnails","/thumbnail/thumbnails"}){Json::json_pointer pointer(path);if(item.contains(pointer)&&item.at(pointer).is_array()&&!item.at(pointer).empty())row.cover=str(item.at(pointer).back(),"/url");}
            if(kind==1){row.albumId=id;row.album=row.title;}else{row.artistId=id;row.artist=row.title;}if(!row.title.empty())rows.push_back(std::move(row));return;
        }for(const auto& child:node.items())walk(child.value());
    };walk(root);return rows;
}
}
