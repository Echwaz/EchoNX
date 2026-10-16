#include "disk_cache.hpp"
#include <cassert>
#include <iostream>
#include <thread>
namespace fs=std::filesystem;
int main(){
    const fs::path root="work/disk-cache-test";fs::create_directories(root);echo::DiskCache cache(root,4096);cache.clear();
    cache.put("lyrics",std::string("hello\0world",11),100);assert(cache.get("lyrics",20,110)==std::string("hello\0world",11));
    echo::DiskCache reopened(root,4096);assert(reopened.get("lyrics",20,119));assert(!reopened.get("lyrics",20,120));assert(!reopened.get("lyrics",20,99));assert(reopened.get("lyrics",-1,1000));
    auto file=fs::directory_iterator(root)->path();auto timestamp=fs::last_write_time(file);assert(cache.get("lyrics",-1));assert(fs::last_write_time(file)==timestamp);{std::fstream out(file,std::ios::binary|std::ios::in|std::ios::out);out.seekp(-1,std::ios::end);out.put('!');}assert(!cache.get("lyrics",-1));cache.clear();
    cache.put("empty","",100);assert(cache.get("empty",60,110).has_value());cache.clear();
    echo::DiskCache limited(root,160);limited.put("a",std::string(40,'a'),100);auto a=fs::directory_iterator(root)->path();limited.put("b",std::string(40,'b'),100);for(const auto& item:fs::directory_iterator(root))if(item.path()!=a)fs::last_write_time(item.path(),fs::file_time_type::clock::now()-std::chrono::hours(1));assert(limited.get("a",100,110));limited.put("c",std::string(40,'c'),100);assert(limited.get("a",100,110));assert(!limited.get("b",100,110));assert(limited.get("c",100,110));limited.clear();
    std::ofstream(root/"library.json")<<"keep";limited.put("x","cached",100);limited.clear();assert(fs::exists(root/"library.json"));fs::remove(root/"library.json");
    std::thread first([&]{for(int i=0;i<30;i++)cache.put("thread-a",std::to_string(i),100);});std::thread second([&]{for(int i=0;i<30;i++){cache.put("thread-b",std::to_string(i),100);cache.get("thread-a",-1);}});first.join();second.join();assert(cache.get("thread-a",-1)=="29"&&cache.get("thread-b",-1)=="29");cache.clear();fs::remove(root);
    std::cout<<"PASS: persisted binary cache, TTL independent of access time, corruption rejection, LRU quota, clear preserves other files, concurrent access.\n";
}