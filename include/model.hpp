#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include <functional>
#include <nlohmann/json.hpp>

namespace echo {
using Json = nlohmann::json;
struct Song { std::string id, title, artist, cover, duration, artistId, albumId, album; };
inline std::string str(const Json& n, const char* path) {
    Json::json_pointer p(path);
    return n.contains(p) && n.at(p).is_string() ? n.at(p).get<std::string>() : "";
}
inline std::string text(const Json& n) {
    if (!n.is_object()) return "";
    if (n.contains("simpleText") && n["simpleText"].is_string()) return n["simpleText"];
    std::string s;
    if (n.contains("runs") && n["runs"].is_array())
        for (const auto& r : n["runs"]) s += str(r,"/text");
    return s;
}
inline std::vector<Song> songs(const Json& root, size_t limit=40) {
    std::vector<Song> out; std::unordered_set<std::string> seen;
    std::function<void(const Json&)> walk = [&](const Json& n) {
        if (out.size() >= limit) return;
        if (n.is_array()) { for (const auto& c:n) walk(c); return; }
        if (!n.is_object()) return;
        for (const auto* key : {"musicResponsiveListItemRenderer", "musicTwoRowItemRenderer", "playlistPanelVideoRenderer"}) {
            if (!n.contains(key) || !n[key].is_object()) continue;
            const auto& r=n[key]; Song s;
            s.id=str(r,"/videoId");
            if(s.id.empty())s.id=str(r,"/playlistItemData/videoId");
            if(s.id.empty())s.id=str(r,"/navigationEndpoint/watchEndpoint/videoId");
            if(s.id.empty())s.id=str(r,"/flexColumns/0/musicResponsiveListItemFlexColumnRenderer/text/runs/0/navigationEndpoint/watchEndpoint/videoId");
            if(s.id.empty())s.id=str(r,"/overlay/musicItemThumbnailOverlayRenderer/content/musicPlayButtonRenderer/playNavigationEndpoint/watchEndpoint/videoId");
            if(s.id.empty() || !seen.insert(s.id).second) return;
            if (r.contains("title")) s.title=text(r["title"]);
            if (s.title.empty() && r.contains(Json::json_pointer("/flexColumns/0/musicResponsiveListItemFlexColumnRenderer/text")))
                s.title=text(r.at(Json::json_pointer("/flexColumns/0/musicResponsiveListItemFlexColumnRenderer/text")));
            for(const char* p:{"/longBylineText/runs/0/text","/subtitle/runs/0/text","/flexColumns/1/musicResponsiveListItemFlexColumnRenderer/text/runs/0/text"})
                if(s.artist.empty())s.artist=str(r,p);
            for(const char* path:{"/longBylineText/runs","/subtitle/runs","/flexColumns/1/musicResponsiveListItemFlexColumnRenderer/text/runs","/flexColumns/2/musicResponsiveListItemFlexColumnRenderer/text/runs"}) {
                Json::json_pointer p(path);if(!r.contains(p)||!r.at(p).is_array())continue;
                for(const auto& run:r.at(p)){auto id=str(run,"/navigationEndpoint/browseEndpoint/browseId");auto type=str(run,"/navigationEndpoint/browseEndpoint/browseEndpointContextSupportedConfigs/browseEndpointContextMusicConfig/pageType");
                    if(type=="MUSIC_PAGE_TYPE_ARTIST" && id.rfind("UC",0)==0){s.artistId=id;s.artist=str(run,"/text");break;}}
                if(!s.artistId.empty())break;
            }
            for(const char* path:{"/longBylineText/runs","/subtitle/runs","/flexColumns/1/musicResponsiveListItemFlexColumnRenderer/text/runs","/flexColumns/2/musicResponsiveListItemFlexColumnRenderer/text/runs"}) {
                Json::json_pointer p(path);if(!r.contains(p)||!r.at(p).is_array())continue;
                for(const auto& run:r.at(p)){auto id=str(run,"/navigationEndpoint/browseEndpoint/browseId");auto type=str(run,"/navigationEndpoint/browseEndpoint/browseEndpointContextSupportedConfigs/browseEndpointContextMusicConfig/pageType");if(type=="MUSIC_PAGE_TYPE_ALBUM"){s.albumId=id;s.album=str(run,"/text");break;}}
                if(!s.albumId.empty())break;
            }
            if(r.contains("lengthText"))s.duration=text(r["lengthText"]);
            const Json::json_pointer subtitle("/flexColumns/1/musicResponsiveListItemFlexColumnRenderer/text/runs");
            if(r.contains(subtitle) && r.at(subtitle).is_array())for(const auto& run:r.at(subtitle)) {
                auto label=str(run,"/text");
                if(label.find(':')!=std::string::npos && label.find_first_not_of("0123456789:")==std::string::npos)s.duration=label;
            }
            for(const char* p:{"/thumbnail/thumbnails","/thumbnail/musicThumbnailRenderer/thumbnail/thumbnails","/thumbnailRenderer/musicThumbnailRenderer/thumbnail/thumbnails"}) {
                Json::json_pointer q(p);
                if(r.contains(q) && r.at(q).is_array() && !r.at(q).empty())s.cover=str(r.at(q).back(),"/url");
            }
            auto size=s.cover.find("=w");if(size!=std::string::npos && s.cover.find("googleusercontent.com")!=std::string::npos)s.cover=s.cover.substr(0,size)+"=w544-h544-l90-rj";
            if(!s.title.empty())out.push_back(s);
            return;
        }
        for (const auto& c:n.items())walk(c.value());
    };
    walk(root);return out;
}
inline Json to_json(const Song& s) {return {{"id",s.id},{"title",s.title},{"artist",s.artist},{"cover",s.cover},{"duration",s.duration},{"artistId",s.artistId},{"albumId",s.albumId},{"album",s.album}};}
inline Song from_json(const Json& j) {return {str(j,"/id"),str(j,"/title"),str(j,"/artist"),str(j,"/cover"),str(j,"/duration"),str(j,"/artistId"),str(j,"/albumId"),str(j,"/album")};}
}
