#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
namespace echo {
class DiskCache {
    std::filesystem::path root_;std::uintmax_t limit_,used_=0;bool indexed_=false;std::mutex mutex_;
    struct Header {std::uint64_t magic=0x4543484f43414331ULL;std::int64_t created=0;std::uint64_t size=0,hash=0;};
    static std::uint64_t hash(const std::string& value){std::uint64_t h=14695981039346656037ULL;for(unsigned char c:value){h^=c;h*=1099511628211ULL;}return h;}
    std::filesystem::path path(const std::string& key){std::ostringstream out;out<<std::hex<<hash(key);return root_/(out.str()+".cache");}
    void index(){if(indexed_)return;used_=0;for(const auto& item:std::filesystem::directory_iterator(root_))if(item.is_regular_file()&&item.path().extension()==".cache")used_+=item.file_size();indexed_=true;}
    void trim(){if(used_<=limit_)return;struct Entry{std::filesystem::path path;std::uintmax_t size;std::filesystem::file_time_type time;};std::vector<Entry> files;used_=0;for(const auto& item:std::filesystem::directory_iterator(root_))if(item.is_regular_file()&&item.path().extension()==".cache"){auto size=item.file_size();used_+=size;files.push_back({item.path(),size,item.last_write_time()});}std::sort(files.begin(),files.end(),[](const Entry& a,const Entry& b){return a.time<b.time;});for(const auto& item:files){if(used_<=limit_)break;if(std::filesystem::remove(item.path))used_-=item.size;}}
public:
    explicit DiskCache(std::filesystem::path root,std::uintmax_t limit=100ULL*1024*1024):root_(std::move(root)),limit_(limit){}
    static std::int64_t now(){return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
    std::optional<std::string> get(const std::string& key,std::int64_t ttl,std::int64_t current=now())noexcept{
        std::lock_guard<std::mutex> lock(mutex_);try{auto file=path(key);std::ifstream in(file,std::ios::binary);Header h;if(!in.read(reinterpret_cast<char*>(&h),sizeof(h))||h.magic!=Header{}.magic||h.size>8ULL*1024*1024||std::filesystem::file_size(file)!=sizeof(h)+h.size)return {};if(ttl>=0&&(current<h.created||current-h.created>=ttl))return {};std::string value(h.size,'\0');if(!in.read(value.data(),value.size())||hash(value)!=h.hash)return {};in.close();std::filesystem::last_write_time(file,std::filesystem::file_time_type::clock::now());return value;}catch(...){return {};}}
    void put(const std::string& key,const std::string& value,std::int64_t current=now())noexcept{
        std::lock_guard<std::mutex> lock(mutex_);try{if(value.size()+sizeof(Header)>limit_||value.size()>8ULL*1024*1024)return;std::filesystem::create_directories(root_);index();auto file=path(key),temp=file;auto oldSize=std::filesystem::exists(file)?std::filesystem::file_size(file):0;temp+=".tmp";Header h;h.created=current;h.size=value.size();h.hash=hash(value);{std::ofstream out(temp,std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(&h),sizeof(h));out.write(value.data(),value.size());out.close();if(!out){std::filesystem::remove(temp);return;}}std::filesystem::remove(file);std::filesystem::rename(temp,file);used_=used_>=oldSize?used_-oldSize:0;used_+=sizeof(h)+value.size();trim();}catch(...) {indexed_=false;}}
    void clear()noexcept{std::lock_guard<std::mutex> lock(mutex_);used_=0;indexed_=false;try{if(!std::filesystem::exists(root_))return;for(const auto& file:std::filesystem::directory_iterator(root_))if(file.is_regular_file()&&(file.path().extension()==".cache"||file.path().extension()==".tmp"))std::filesystem::remove(file.path());}catch(...) {}}
};
inline DiskCache& disk_cache(){
#ifdef __SWITCH__
    static DiskCache cache("sdmc:/switch/echonx/cache");
#else
    static DiskCache cache("cache");
#endif
    return cache;
}
}