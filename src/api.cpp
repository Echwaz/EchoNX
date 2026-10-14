#include "api.hpp"
#include <curl/curl.h>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <cctype>
#include "recommendations.hpp"

namespace echo {
namespace {
struct Download {std::string data;size_t max;std::atomic<bool>* abort;};
size_t receive(char* p,size_t a,size_t b,void* u){auto& d=*static_cast<Download*>(u);size_t size=a*b;if(d.data.size()+size>d.max)return 0;d.data.append(p,size);return size;}
int progress(void* u,curl_off_t,curl_off_t,curl_off_t,curl_off_t){auto* a=static_cast<std::atomic<bool>*>(u);return a && a->load()?1:0;}
}
std::string http(const std::string& url,const std::string& body,const std::vector<std::string>& headers,std::atomic<bool>* abort,const char* agent,size_t maxBytes){
    CURL* c=curl_easy_init();if(!c)throw std::runtime_error("HTTP initialization failed");
    Download d{{},maxBytes,abort};curl_slist* h=nullptr;for(const auto& s:headers)h=curl_slist_append(h,s.c_str());
    curl_easy_setopt(c,CURLOPT_URL,url.c_str());curl_easy_setopt(c,CURLOPT_USERAGENT,agent);
    curl_easy_setopt(c,CURLOPT_HTTPHEADER,h);curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,1L);
    curl_easy_setopt(c,CURLOPT_CONNECTTIMEOUT,10L);curl_easy_setopt(c,CURLOPT_TIMEOUT,30L);
    curl_easy_setopt(c,CURLOPT_NOSIGNAL,1L);curl_easy_setopt(c,CURLOPT_ACCEPT_ENCODING,"");
    curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,receive);curl_easy_setopt(c,CURLOPT_WRITEDATA,&d);
    curl_easy_setopt(c,CURLOPT_XFERINFOFUNCTION,progress);curl_easy_setopt(c,CURLOPT_XFERINFODATA,abort);
    curl_easy_setopt(c,CURLOPT_NOPROGRESS,0L);
#ifdef __SWITCH__
    curl_easy_setopt(c,CURLOPT_CAINFO,"romfs:/cacert.pem");
#endif
    if(!body.empty()){curl_easy_setopt(c,CURLOPT_POSTFIELDS,body.c_str());curl_easy_setopt(c,CURLOPT_POSTFIELDSIZE,static_cast<long>(body.size()));}
    CURLcode result=curl_easy_perform(c);long status=0;curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&status);
    curl_slist_free_all(h);curl_easy_cleanup(c);
    if(result!=CURLE_OK)throw std::runtime_error(std::string("Network: ")+curl_easy_strerror(result));
    if(status<200 || status>=300)throw std::runtime_error("HTTP "+std::to_string(status)+". Try another song or retry.");
    return d.data;
}
Json Api::context(){Json c={{"client",{{"clientName","WEB_REMIX"},{"clientVersion","1.20260213.01.00"},{"hl","en"},{"gl","US"}}}};if(!visitor_.empty())c["client"]["visitorData"]=visitor_;return c;}
Json Api::call(const std::string& endpoint,Json payload){
    payload["context"]=context();std::vector<std::string> h={"Content-Type: application/json","Origin: https://music.youtube.com","Referer: https://music.youtube.com/","X-YouTube-Client-Name: 67","X-YouTube-Client-Version: 1.20260213.01.00"};
    if(!visitor_.empty())h.push_back("X-Goog-Visitor-Id: "+visitor_);
    auto root=Json::parse(http("https://music.youtube.com/youtubei/v1/"+endpoint+"?prettyPrint=false",payload.dump(),h));
    auto v=str(root,"/responseContext/visitorData");if(!v.empty())visitor_=v;return root;
}
std::vector<Song> Api::home(){std::lock_guard<std::mutex> l(mutex_);return songs(call("browse",{{"browseId","FEmusic_home"}}));}
std::vector<Song> Api::search(const std::string& q){std::lock_guard<std::mutex> l(mutex_);return songs(call("search",{{"query",q},{"params","EgWKAQIIAWoKEAkQBRAKEAMQBA%3D%3D"}}));}
std::vector<Song> Api::related(const std::string& id){std::lock_guard<std::mutex> l(mutex_);return songs(call("next",{{"videoId",id},{"playlistId","RDAMVM"+id},{"isAudioOnly",true}}));}
std::vector<Song> Api::for_saved(const std::vector<Song>& saved,size_t offset){
    if(saved.empty())return home();std::vector<std::vector<Song>> feeds;std::string lastError;
    for(const auto& seed:recommendation_seeds(saved,offset))try{feeds.push_back(related(seed.id));}catch(const std::exception& e){lastError=e.what();}
    if(feeds.empty())throw std::runtime_error("Saved-song recommendations unavailable. "+lastError);
    return rank_recommendations(saved,feeds);
}
std::vector<Song> Api::artist(const Song& selected){
    Song anchor=selected;
    if(anchor.artistId.empty()){
        for(const auto& s:search(anchor.artist))if(s.artist==anchor.artist && !s.artistId.empty()){anchor.artistId=s.artistId;break;}
    }
    if(anchor.artistId.empty())throw std::runtime_error("Artist page not available for this song. Try a result from Search.");
    std::lock_guard<std::mutex> l(mutex_);auto root=call("browse",{{"browseId",anchor.artistId}});Json shelf;
    std::function<void(const Json&)> find=[&](const Json& n){if(!shelf.is_null())return;if(n.is_array()){for(const auto& c:n)find(c);}else if(n.is_object()){
        if(n.contains("musicShelfRenderer")){const auto& r=n["musicShelfRenderer"];if(r.is_object()&&r.contains("title")&&(text(r["title"])=="Songs"||text(r["title"])=="Top songs")){shelf=r;return;}}
        for(const auto& c:n.items())find(c.value());}};find(root);
    if(shelf.is_null())throw std::runtime_error("This artist page did not return a Songs section.");
    auto id=str(shelf,"/bottomEndpoint/browseEndpoint/browseId"),params=str(shelf,"/bottomEndpoint/browseEndpoint/params");
    if(id.empty()){id=str(shelf,"/title/runs/0/navigationEndpoint/browseEndpoint/browseId");params=str(shelf,"/title/runs/0/navigationEndpoint/browseEndpoint/params");}
    auto rows=songs(shelf,60);
    if(!id.empty())try{Json payload={{"browseId",id}};if(!params.empty())payload["params"]=params;auto full=songs(call("browse",payload),120);if(!full.empty())rows=std::move(full);}catch(...){}
    for(auto& s:rows)if(s.artistId.empty()){s.artist=anchor.artist;s.artistId=anchor.artistId;}
    return rows;
}
std::vector<Song> Api::album(const Song& selected){
    Song anchor=selected;
    if(anchor.albumId.empty())for(const auto& song:search(anchor.title+" "+anchor.artist))if(song.id==anchor.id&&!song.albumId.empty()){anchor.albumId=song.albumId;anchor.album=song.album;break;}
    if(anchor.albumId.empty())throw std::runtime_error("No album page is attached to this song.");
    std::lock_guard<std::mutex> lock(mutex_);auto rows=songs(call("browse",{{"browseId",anchor.albumId}}),120);
    for(auto& song:rows){song.albumId=anchor.albumId;song.album=anchor.album;if(song.artistId.empty()){song.artist=anchor.artist;song.artistId=anchor.artistId;}}
    return rows;
}
Lyrics Api::lyrics(const Song& song,std::atomic<bool>* abort){
    auto escape=[](const std::string& s){std::string out;const char* hex="0123456789ABCDEF";for(unsigned char c:s){if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')out+=c;else{out+='%';out+=hex[c>>4];out+=hex[c&15];}}return out;};
    auto normalize=[](const std::string& s){std::string out;for(unsigned char c:s)if(c>127||std::isalnum(c))out+=c<128?char(std::tolower(c)):char(c);return out;};
    int duration=0;std::istringstream time(song.duration);std::string part;try{while(std::getline(time,part,':'))duration=duration*60+std::stoi(part);}catch(...){duration=0;}
    const std::string query="track_name="+escape(song.title)+"&artist_name="+escape(song.artist);
    const char* agent="EchoNX/0.3 (https://lrclib.net)";
    if(duration>0)try{auto root=Json::parse(http("https://lrclib.net/api/get?"+query+"&album_name="+escape(song.album)+"&duration="+std::to_string(duration),"",{},abort,agent));return parse_lyrics(root);}catch(...){if(abort&&abort->load())return {{},{},"Lyrics request cancelled"};}
    auto matches=Json::parse(http("https://lrclib.net/api/search?"+query,"",{},abort,agent));
    if(matches.is_array())for(const auto& item:matches){if(normalize(str(item,"/trackName"))!=normalize(song.title)||normalize(str(item,"/artistName"))!=normalize(song.artist))continue;if(duration>0&&std::fabs(item.value("duration",0.0)-duration)>5)continue;auto result=parse_lyrics(item);if(!result.lines.empty()||!result.plain.empty()||result.status=="Instrumental")return result;}
    return {{},{},"Lyrics unavailable for this song"};
}
Stream Api::resolve(const std::string& id,std::atomic<bool>* abort){
    std::lock_guard<std::mutex> l(mutex_);
    if(visitor_.empty())call("browse",{{"browseId","FEmusic_home"}});
    Json client={{"clientName","VISIONOS"},{"clientVersion","1.02"},{"hl","en"},{"gl","US"},{"osName","visionOS"},{"osVersion","1.3.21O771"},{"deviceMake","Apple"},{"deviceModel","RealityDevice14,1"},{"visitorData",visitor_}};
    Json p={{"context",{{"client",client}}},{"videoId",id},{"contentCheckOk",true},{"racyCheckOk",true}};
    auto root=Json::parse(http("https://music.youtube.com/youtubei/v1/player?prettyPrint=false",p.dump(),{"Content-Type: application/json","X-YouTube-Client-Name: 101","X-YouTube-Client-Version: 1.02","X-Goog-Visitor-Id: "+visitor_},abort,visionUA));
    if(str(root,"/playabilityStatus/status")!="OK")throw std::runtime_error(str(root,"/playabilityStatus/reason"));
    const Json::json_pointer path("/streamingData/adaptiveFormats");
    if(!root.contains(path) || !root.at(path).is_array())throw std::runtime_error("No audio stream returned");
    Stream best;int score=-1;
    for(const auto& f:root.at(path)){
        auto mime=str(f,"/mimeType"),url=str(f,"/url"),len=str(f,"/contentLength");
        if(mime.rfind("audio/",0)!=0 || url.empty() || len.empty())continue;
        int rank=(mime.find("audio/mp4")!=std::string::npos?1000000:0)+f.value("bitrate",0);
        if(rank>score){score=rank;best={url,std::stoll(len),visionUA};}
    }
    if(best.url.empty())throw std::runtime_error("Direct audio unavailable for this song");
    return best;
}
}
