#include "library_store.hpp"
#include <iostream>
#include <cerrno>
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
    std::string path="tests/store-case/library.json";
    std::filesystem::create_directories("tests/store-case");
    for(const auto& file:{path,path+".bak",path+".tmp"})std::filesystem::remove(file);
    auto switchRename=[](const std::string& from,const std::string& to){if(std::filesystem::exists(to)){errno=EEXIST;return -1;}return std::rename(from.c_str(),to.c_str());};
    std::vector<echo::Song> rows;
    for(int i=0;i<5;i++){rows.push_back({std::to_string(i),"Saved track","Artist","","","UCartist"});echo::save_library(path,rows,switchRename);check(echo::load_library(path).size()==rows.size(),"Repeated save failed on no-overwrite filesystem");}
    auto failReplacement=[&](const std::string& from,const std::string& to){if(from==path+".tmp"){errno=EIO;return -1;}return switchRename(from,to);};
    bool failed=false;try{echo::save_library(path,{},failReplacement);}catch(...){failed=true;}
    check(failed && echo::load_library(path).size()==5,"Failed update lost existing saves");
    auto failRestore=[&](const std::string& from,const std::string& to){if(from==path+".tmp"||from==path+".bak")return -1;return switchRename(from,to);};
    try{echo::save_library(path,{},failRestore);}catch(...){}
    check(echo::load_library(path).size()==5,"Backup recovery failed");
    echo::save_library(path,rows,switchRename);check(echo::load_library(path).size()==5,"Save after recovery failed");
    for(const auto& file:{path,path+".bak",path+".tmp"})std::filesystem::remove(file);
    std::filesystem::remove("tests/store-case");
    std::cout<<"PASS: repeated Switch-style saves, failed replacement rollback and backup recovery\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
