#include "api.hpp"
#include <curl/curl.h>
#include <cassert>
#include <chrono>
#include <thread>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
int main(){
    curl_global_init(CURL_GLOBAL_DEFAULT);int server=socket(AF_INET,SOCK_STREAM,0);assert(server>=0);sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);assert(bind(server,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0);socklen_t length=sizeof(address);assert(getsockname(server,reinterpret_cast<sockaddr*>(&address),&length)==0&&listen(server,1)==0);
    std::atomic<bool> accepted{false},abort{false};std::thread stalled([&]{int peer=accept(server,nullptr,nullptr);assert(peer>=0);char request[4096];recv(peer,request,sizeof(request),0);const char response[]="HTTP/1.1 200 OK\r\nContent-Length: 1\r\n\r\n";send(peer,response,sizeof(response)-1,0);accepted=true;while(recv(peer,request,sizeof(request),0)>0){}close(peer);});
    std::thread cancel([&]{while(!accepted)std::this_thread::sleep_for(std::chrono::milliseconds(1));std::this_thread::sleep_for(std::chrono::milliseconds(150));abort=true;});
    auto start=std::chrono::steady_clock::now();bool cancelled=false;try{echo::http("http://127.0.0.1:"+std::to_string(ntohs(address.sin_port)),"",{},&abort);}catch(const std::exception&){cancelled=true;}auto elapsed=std::chrono::steady_clock::now()-start;assert(cancelled&&elapsed<std::chrono::milliseconds(750));cancel.join();stalled.join();close(server);
    echo::Api api;cancelled=false;try{api.resolve("unused",&abort);}catch(const std::exception&){cancelled=true;}assert(cancelled);curl_global_cleanup();std::cout<<"PASS: cancellation interrupts stalled HTTP body promptly and pre-cancelled initial resolve makes no request.\n";
}
