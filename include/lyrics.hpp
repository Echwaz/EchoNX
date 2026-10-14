#pragma once
#include "model.hpp"
#include <sstream>
#include <algorithm>
#include <cstdio>

namespace echo {
struct LyricLine{double time;std::string text;};
struct Lyrics{std::vector<LyricLine> lines;std::vector<std::string> plain;std::string status;};
inline Lyrics parse_lyrics(const Json& root){
    Lyrics result;if(root.value("instrumental",false)){result.status="Instrumental";return result;}
    std::istringstream input(str(root,"/syncedLyrics"));std::string line;double offset=0;
    while(std::getline(input,line)){
        if(line.rfind("[offset:",0)==0){try{offset=std::stod(line.substr(8))/1000;}catch(...){}continue;}
        std::vector<double> times;size_t pos=0;
        while(pos<line.size()&&line[pos]=='['){auto end=line.find(']',pos);if(end==std::string::npos)break;int min=0;double sec=0;
            if(std::sscanf(line.substr(pos+1,end-pos-1).c_str(),"%d:%lf",&min,&sec)==2&&min>=0&&sec>=0&&sec<60)times.push_back(min*60+sec);pos=end+1;
        }
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        for(double time:times)result.lines.push_back({time,line.substr(pos)});
    }
    for(auto& row:result.lines)row.time=std::max(0.0,row.time+offset);
    std::stable_sort(result.lines.begin(),result.lines.end(),[](const auto& a,const auto& b){return a.time<b.time;});
    if(!result.lines.empty()){result.status="Synced lyrics";return result;}
    std::istringstream plain(str(root,"/plainLyrics"));while(std::getline(plain,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(!line.empty())result.plain.push_back(line);}
    result.status=result.plain.empty()?"Lyrics unavailable for this song":"Lyrics / no timestamps";return result;
}
inline int lyric_index(const Lyrics& lyrics,double time){int selected=-1;for(size_t i=0;i<lyrics.lines.size()&&lyrics.lines[i].time<=time;i++)selected=int(i);return selected;}
}
