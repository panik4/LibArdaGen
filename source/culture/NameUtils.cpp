#include "culture/NameUtils.h"
namespace Arda::Names {

std::string generateTag(const std::string name,
                        std::set<std::string> &disallowedTokens) {
  std::string tag{""};
  int retries = 0;
  // all letters in the alphabet
  const std::vector<std::string> letters{
      "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
      "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z"};

  do {
    tag = name.substr(0, std::min<int>(3, name.size()));
    std::transform(tag.begin(), tag.end(), tag.begin(), ::toupper);
    if (tag.size() < 3)
      tag += Fwg::Utils::selectRandom(letters)[0];
    // if we have a retry, simply replace one of the letters with a random one
    if (retries > 0)
      tag[RandNum::getRandom(0, 2)] = Fwg::Utils::selectRandom(letters)[0];
  } while (disallowedTokens.find(tag) !=
               disallowedTokens.end() &&
           retries++ < 10);
  if (retries >= 10) {
    do {
      // add a random letter to the tag
      tag.resize(3);
      tag[2] = Fwg::Utils::selectRandom(letters)[0];
    } while (disallowedTokens.find(tag) !=
                 disallowedTokens.end() &&
             retries++ < 20);
  }
  if (tag.size() != 3) {
    std::cerr << "Incorrect tag size" << std::endl;
    throw(std::exception(
        std::string("Incorrect tag size in generating tag " + tag).c_str()));
  }
  if (retries >= 20)
    throw(std::exception(
        std::string("Too many tries generating tag " + tag).c_str()));

  disallowedTokens.insert(tag);

  return tag;
}



} // namespace Arda