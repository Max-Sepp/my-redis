#ifndef MYREDIS_CLIENT_STRING_HELPERS_H_
#define MYREDIS_CLIENT_STRING_HELPERS_H_

#include <string>
#include <vector>

namespace myredis {

std::vector<std::string> Split(const std::string& delimiter,
                               const std::string& input);
std::vector<std::string> JoinDelimitedStrings(
    const std::vector<std::string>& input);

}  // namespace myredis

#endif  // MYREDIS_CLIENT_STRING_HELPERS_H_
