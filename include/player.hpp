#pragma once
#include "api.hpp"
#include <SDL2/SDL.h>
#include <thread>

namespace echo {
class Player {
    Api& api_;SDL_AudioDeviceID device_=0;std::thread worker_;
    std::atomic<bool> abort_{false},paused_{false},done_{false},active_{false},started_{false};
    std::atomic<double> position_{0},duration_{0},seek_{-1};
    mutable std::mutex mutex_;std::string status_="Choose a song";
    void decode(const Song& song);void status(const std::string& s);
public:
    explicit Player(Api& api);~Player();
    void play(const Song& song,double startSeconds=0);void stop();void toggle();void seek(double seconds);
    bool paused()const{return paused_;}bool active()const{return active_;}
    bool hasStarted()const{return started_;}
    bool ended(){return done_.exchange(false);}
    double position()const{return position_;}double duration()const{return duration_;}
    std::string status()const;
};
}
