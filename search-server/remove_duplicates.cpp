#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server)
{
	std::set<int> ids_to_delete;
	for (const int id : search_server)
	{
		if (ids_to_delete.count(id))
			continue;
		std::set<int> duplicates = FindDuplicates(search_server, id);
		for (int duplicate_id : duplicates)
		{
			ids_to_delete.insert(duplicate_id);
		}
	}

	for (const int id : ids_to_delete)
	{
		std::cout << "Found duplicate document id "s << id << '\n';
		search_server.RemoveDocument(id);
	}
}

std::set<int> FindDuplicates(SearchServer& search_server, int id)
{
	std::map<std::string, double> current_words = search_server.GetWordFrequences(id);
	std::set<int> duplicates {id};
	for (const int it_id : search_server)
	{
		std::map<std::string, double> it_words = search_server.GetWordFrequences(it_id);
		if (MapKeysEqual(current_words, it_words))
		{
			duplicates.insert(it_id);
		}
	}

	duplicates.erase(duplicates.begin());
	return duplicates;
}

bool MapKeysEqual(const std::map<std::string, double>& m1, const std::map<std::string, double>& m2)
{
	if (m1.size() != m2.size())
		return false;

	for (const auto& [word, _] : m1)
		if (m2.count(word) == 0)
			return false;

	return true;
}