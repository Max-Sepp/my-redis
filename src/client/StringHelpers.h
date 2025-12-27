#ifndef MY_REDIS_STRINGHELPERS_H
#define MY_REDIS_STRINGHELPERS_H

#include <string>
#include <vector>

namespace StringHelpers {
std::vector<std::string> Split(const std::string& delimiter,
                               const std::string& input);
std::vector<std::string> JoinDelimitedStrings(
    const std::vector<std::string>& input);
}  // namespace StringHelpers

#endif  // MY_REDIS_STRINGHELPERS_H
