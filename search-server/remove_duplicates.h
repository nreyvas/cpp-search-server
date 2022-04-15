#pragma once
#include "search_server.h"

void RemoveDuplicates(SearchServer& search_server);

std::set<int> FindDuplicates(SearchServer& search_server, int id);

bool MapKeysEqual(const std::map<std::string_view, double>& m1,
	const std::map<std::string_view, double>& m2);