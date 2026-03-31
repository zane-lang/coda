#pragma once
#include <vector>
#include <unordered_map>

#include <stdexcept>
#include <utility>
#include <algorithm>
#include <functional>

namespace coda {
namespace detail {

/// A map that preserves insertion order.
/// Iteration follows the order elements were inserted.
/// Key lookup is O(1) via an internal hash map.
template <typename K, typename V>
class OrderedMap {
	std::vector<std::pair<K, V>> entries;
	std::unordered_map<K, size_t> index; // key → position in entries

public:
	OrderedMap() = default;

	V& operator[](const K& key) {
		auto it = index.find(key);
		if (it != index.end())
			return entries[it->second].second;
		index[key] = entries.size();
		entries.emplace_back(key, V{});
		return entries.back().second;
	}

	const V& at(const K& key) const {
		auto it = index.find(key);
		if (it == index.end())
			throw std::out_of_range("OrderedMap::at — key not found: " + key);
		return entries[it->second].second;
	}

	V& at(const K& key) {
		auto it = index.find(key);
		if (it == index.end())
			throw std::out_of_range("OrderedMap::at — key not found: " + key);
		return entries[it->second].second;
	}

	bool contains(const K& key) const {
		return index.find(key) != index.end();
	}

	size_t count(const K& key) const {
		return index.count(key);
	}

	size_t size() const { return entries.size(); }
	bool empty() const { return entries.empty(); }

	void clear() {
		entries.clear();
		index.clear();
	}

	void erase(const K& key) {
		auto it = index.find(key);
		if (it == index.end()) return;
		size_t pos = it->second;
		index.erase(it);
		entries.erase(entries.begin() + pos);
		for (size_t i = pos; i < entries.size(); ++i)
			index[entries[i].first] = i;
	}

	using iterator       = typename std::vector<std::pair<K, V>>::iterator;
	using const_iterator = typename std::vector<std::pair<K, V>>::const_iterator;

	iterator       begin()       { return entries.begin(); }
	iterator       end()         { return entries.end(); }
	const_iterator begin() const { return entries.begin(); }
	const_iterator end()   const { return entries.end(); }

	/// Sort entries by key (alphabetical). Rebuilds the index.
	void sort() {
		std::stable_sort(entries.begin(), entries.end(),
			[](const auto& a, const auto& b) { return a.first < b.first; });
		rebuildIndex();
	}

	/// Sort with a predicate that classifies values as containers.
	/// Scalars come first (alphabetical), then containers (alphabetical).
	template <typename Pred>
	void sort(Pred isContainer) {
		std::stable_sort(entries.begin(), entries.end(),
			[&](const auto& a, const auto& b) {
				bool aCont = isContainer(a.second);
				bool bCont = isContainer(b.second);
				if (aCont != bCont) return !aCont;
				return a.first < b.first;
			});
		rebuildIndex();
	}

	/// Sort by a weight function on the key.
	/// Higher weight → closer to the top.
	/// Equal weight → alphabetical by key.
	void sortByWeight(const std::function<float(const K&)>& weightFn) {
		std::stable_sort(entries.begin(), entries.end(),
			[&](const auto& a, const auto& b) {
				float wa = weightFn(a.first);
				float wb = weightFn(b.first);
				if (wa != wb) return wa > wb;
				return a.first < b.first;
			});
		rebuildIndex();
	}

	/// Insert a key-value pair. Returns {iterator, true} if inserted,
	/// {iterator, false} if the key already existed (value unchanged).
	std::pair<iterator, bool> insert(const K& key, V value) {
		auto it = index.find(key);
		if (it != index.end())
			return { entries.begin() + it->second, false };
		index[key] = entries.size();
		entries.emplace_back(key, std::move(value));
		return { entries.end() - 1, true };
	}

private:
	void rebuildIndex() {
		index.clear();
		for (size_t i = 0; i < entries.size(); ++i)
			index[entries[i].first] = i;
	}
};

} // namespace detail
} // namespace coda
