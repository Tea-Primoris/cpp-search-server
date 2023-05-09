#pragma once
#include <vector>
#include <string>

std::vector<std::string> SplitIntoWords(const std::string& text);
std::vector<std::string_view> SplitIntoWords(const std::string_view text);