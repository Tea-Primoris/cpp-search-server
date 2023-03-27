#pragma once
#include <algorithm>
#include <execution>
#include <vector>
#include "document.h"
#include "search_server.h"

std::vector<std::vector<Document>> ProcessQueries(
        const SearchServer& search_server,
        const std::vector<std::string>& queries);