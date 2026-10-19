#pragma once

#include <string>
class Auth {
 public:
  // TODO: implement auth methods
  // returns a ticket
  std::string login(std::string username, std::string password);
  std::string signup(std::string username, std::string password);
};