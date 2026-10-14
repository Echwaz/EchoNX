#pragma once
#include "model.hpp"
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <stdexcept>

namespace echo {
inline std::vector<Song> load_library(const std::string& path){
    for(const auto& candidate:{path,path+".bak"}){
        std::ifstream input(candidate);if(!input)continue;
        auto root=Json::parse(input,nullptr,false);if(!root.is_array())continue;
        std::vector<Song> rows;for(const auto& row:root){auto song=from_json(row);if(!song.id.empty())rows.push_back(song);}return rows;
    }
    return {};
}
using RenameFile=std::function<int(const std::string&,const std::string&)>;
inline void save_library(const std::string& path,const std::vector<Song>& rows,
    RenameFile rename=[](const std::string& from,const std::string& to){return std::rename(from.c_str(),to.c_str());}){
    const auto temporary=path+".tmp",backup=path+".bak";
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    Json root=Json::array();for(const auto& row:rows)root.push_back(to_json(row));
    std::ofstream output(temporary,std::ios::trunc);output<<root.dump(2);output.close();
    if(!output)throw std::runtime_error("Cannot write library on SD. Check free space.");
    bool hadOriginal=std::filesystem::exists(path);
    if(hadOriginal){
        std::error_code error;std::filesystem::remove(backup,error);
        if(error)throw std::runtime_error("Cannot prepare library backup on SD.");
        if(rename(path,backup)!=0)throw std::runtime_error("Cannot back up library on SD.");
    }
    // Switch filesystem rename cannot replace an existing destination.
    if(rename(temporary,path)!=0){
        if(hadOriginal)rename(backup,path);
        throw std::runtime_error("Cannot replace library on SD. Previous saves are preserved.");
    }
}
}
