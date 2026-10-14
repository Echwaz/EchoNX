#include "player.hpp"
namespace echo {
Player::Player(Api& a):api_(a){}
Player::~Player(){}
void Player::play(const Song&){}
void Player::stop(){}
void Player::toggle(){}
void Player::seek(double){}
void Player::status(const std::string& s){status_=s;}
std::string Player::status()const{return "Preview from captured YouTube Music data / audio not running";}
void Player::decode(const Song&){}
}
