#include "StringHelpers.h"

namespace StringHelpers {
std::vector<std::string> Split(const std::string& delimiter,
                               const std::string& input) {
  std::vector<std::string> split_up_strings;
  size_t current = 0;
  while (current < input.size()) {
    const size_t next = input.find(delimiter, current);
    split_up_strings.push_back(input.substr(current, next - current));
    if (next == std::string::npos)
      current = input.size();
    else
      current = next + delimiter.size();
  }
  return split_up_strings;
}

/**
 * Takes a vector such as ["test", "\"hello", "world\""] and outputs
 * ["test", "\"hello world\""]
 */
std::vector<std::string> JoinDelimitedStrings(
    const std::vector<std::string>& input) {
  std::vector<std::string> result;
  std::string acc;
  for (const std::string& word : input) {
    if (acc != std::string("")) {
      acc += " " + word;

      if (acc[acc.size() - 1] == '\"') {
        result.push_back(acc.substr(1, acc.size() - 2));
        acc = "";
      }
    } else if (!word.empty() && word[0] == '\"') {
      acc = word;
    } else {
      result.push_back(word);
    }
  }
  if (!acc.empty()) {
    result.push_back(acc);
  }
  return result;
}
}  // namespace StringHelpers
