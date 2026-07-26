// Safety layer for the C FFI.
//
// The implementation arena uses compact, recyclable internal node IDs. Exposing
// those IDs directly made stale handles alias newly allocated nodes and allowed
// callers to attach one node to multiple parents or even another document.
// This translation unit keeps the existing implementation private, then exposes
// stable, document-scoped external handles with ownership validation.

#define coda_doc_new coda_unsafe_doc_new
#define coda_doc_free coda_unsafe_doc_free
#define coda_doc_parse coda_unsafe_doc_parse
#define coda_doc_parse_file coda_unsafe_doc_parse_file
#define coda_doc_parse_fp coda_unsafe_doc_parse_fp
#define coda_doc_order coda_unsafe_doc_order
#define coda_doc_order_weighted coda_unsafe_doc_order_weighted
#define coda_doc_root coda_unsafe_doc_root
#define coda_node_kind coda_unsafe_node_kind
#define coda_node_is_container coda_unsafe_node_is_container
#define coda_node_comment_get coda_unsafe_node_comment_get
#define coda_node_comment_set coda_unsafe_node_comment_set
#define coda_node_header_comment_get coda_unsafe_node_header_comment_get
#define coda_node_header_comment_set coda_unsafe_node_header_comment_set
#define coda_string_get coda_unsafe_string_get
#define coda_string_set coda_unsafe_string_set
#define coda_array_len coda_unsafe_array_len
#define coda_array_get coda_unsafe_array_get
#define coda_array_set coda_unsafe_array_set
#define coda_array_push coda_unsafe_array_push
#define coda_array_remove coda_unsafe_array_remove
#define coda_map_len coda_unsafe_map_len
#define coda_map_key_at coda_unsafe_map_key_at
#define coda_map_value_at coda_unsafe_map_value_at
#define coda_map_get coda_unsafe_map_get
#define coda_map_get_or_insert coda_unsafe_map_get_or_insert
#define coda_map_set coda_unsafe_map_set
#define coda_map_remove coda_unsafe_map_remove
#define coda_table_col_count coda_unsafe_table_col_count
#define coda_table_col_name coda_unsafe_table_col_name
#define coda_table_col_append coda_unsafe_table_col_append
#define coda_table_row_count coda_unsafe_table_row_count
#define coda_table_row_at coda_unsafe_table_row_at
#define coda_table_row_append coda_unsafe_table_row_append
#define coda_table_row_set coda_unsafe_table_row_set
#define coda_table_row_remove coda_unsafe_table_row_remove
#define coda_keyed_table_col_count coda_unsafe_keyed_table_col_count
#define coda_keyed_table_col_name coda_unsafe_keyed_table_col_name
#define coda_keyed_table_col_append coda_unsafe_keyed_table_col_append
#define coda_keyed_table_row_count coda_unsafe_keyed_table_row_count
#define coda_keyed_table_row_key_at coda_unsafe_keyed_table_row_key_at
#define coda_keyed_table_row_at coda_unsafe_keyed_table_row_at
#define coda_keyed_table_row_get coda_unsafe_keyed_table_row_get
#define coda_keyed_table_row_set coda_unsafe_keyed_table_row_set
#define coda_keyed_table_row_remove coda_unsafe_keyed_table_row_remove
#define coda_row_get coda_unsafe_row_get
#define coda_row_set coda_unsafe_row_set
#define coda_row_remove coda_unsafe_row_remove
#define coda_row_col_count coda_unsafe_row_col_count
#define coda_row_col_name_at coda_unsafe_row_col_name_at
#define coda_row_col_value_at coda_unsafe_row_col_value_at
#define coda_row_comment_get coda_unsafe_row_comment_get
#define coda_row_comment_set coda_unsafe_row_comment_set
#define coda_node_serialize coda_unsafe_node_serialize
#define coda_node_order coda_unsafe_node_order
#define coda_node_order_weighted coda_unsafe_node_order_weighted
#define coda_new_string coda_unsafe_new_string
#define coda_new_block coda_unsafe_new_block
#define coda_new_array coda_unsafe_new_array
#define coda_new_table coda_unsafe_new_table
#define coda_new_keyed_table coda_unsafe_new_keyed_table
#define coda_new_row coda_unsafe_new_row
#include "coda_ffi.cpp"
#undef coda_doc_new
#undef coda_doc_free
#undef coda_doc_parse
#undef coda_doc_parse_file
#undef coda_doc_parse_fp
#undef coda_doc_order
#undef coda_doc_order_weighted
#undef coda_doc_root
#undef coda_node_kind
#undef coda_node_is_container
#undef coda_node_comment_get
#undef coda_node_comment_set
#undef coda_node_header_comment_get
#undef coda_node_header_comment_set
#undef coda_string_get
#undef coda_string_set
#undef coda_array_len
#undef coda_array_get
#undef coda_array_set
#undef coda_array_push
#undef coda_array_remove
#undef coda_map_len
#undef coda_map_key_at
#undef coda_map_value_at
#undef coda_map_get
#undef coda_map_get_or_insert
#undef coda_map_set
#undef coda_map_remove
#undef coda_table_col_count
#undef coda_table_col_name
#undef coda_table_col_append
#undef coda_table_row_count
#undef coda_table_row_at
#undef coda_table_row_append
#undef coda_table_row_set
#undef coda_table_row_remove
#undef coda_keyed_table_col_count
#undef coda_keyed_table_col_name
#undef coda_keyed_table_col_append
#undef coda_keyed_table_row_count
#undef coda_keyed_table_row_key_at
#undef coda_keyed_table_row_at
#undef coda_keyed_table_row_get
#undef coda_keyed_table_row_set
#undef coda_keyed_table_row_remove
#undef coda_row_get
#undef coda_row_set
#undef coda_row_remove
#undef coda_row_col_count
#undef coda_row_col_name_at
#undef coda_row_col_value_at
#undef coda_row_comment_get
#undef coda_row_comment_set
#undef coda_node_serialize
#undef coda_node_order
#undef coda_node_order_weighted
#undef coda_new_string
#undef coda_new_block
#undef coda_new_array
#undef coda_new_table
#undef coda_new_keyed_table
#undef coda_new_row

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

struct SafeDocState {
	std::mutex mutex;
	std::unordered_map<coda_node_t, uint32_t> externalToInternal;
	std::unordered_map<uint32_t, coda_node_t> internalToExternal;
	std::unordered_map<uint32_t, uint32_t> parent;
};

std::mutex gRegistryMutex;
std::unordered_map<const coda_doc_t*, std::shared_ptr<SafeDocState>> gStates;
std::atomic<coda_node_t> gNextHandle{1};

coda_node_t allocateHandle() {
	coda_node_t handle = gNextHandle.fetch_add(1, std::memory_order_relaxed);
	if (handle == 0)
		handle = gNextHandle.fetch_add(1, std::memory_order_relaxed);
	return handle;
}

coda_node_t wrapInternal(SafeDocState& state, uint32_t internal) {
	if (internal == 0) return 0;
	if (auto it = state.internalToExternal.find(internal); it != state.internalToExternal.end())
		return it->second;
	const coda_node_t external = allocateHandle();
	state.internalToExternal[internal] = external;
	state.externalToInternal[external] = internal;
	state.parent.try_emplace(internal, 0);
	return external;
}

uint32_t unwrapExternal(const SafeDocState& state, coda_node_t external) {
	if (external == 0) return 0;
	auto it = state.externalToInternal.find(external);
	return it == state.externalToInternal.end() ? 0 : it->second;
}

void registerSubtree(coda_doc_t* doc, SafeDocState& state, uint32_t internal, uint32_t parent) {
	if (!doc || internal == 0) return;
	const auto* node = doc->get(internal);
	if (!node || node->kind == coda_doc::Kind::Null) return;
	wrapInternal(state, internal);
	state.parent[internal] = parent;
	switch (node->kind) {
		case coda_doc::Kind::Block:
		case coda_doc::Kind::KeyedTable:
			for (const auto& [_, child] : node->entries)
				registerSubtree(doc, state, child, internal);
			break;
		case coda_doc::Kind::Array:
		case coda_doc::Kind::Table:
			for (uint32_t child : node->arr)
				registerSubtree(doc, state, child, internal);
			break;
		default:
			break;
	}
}

void registerDocument(coda_doc_t* doc) {
	if (!doc) return;
	auto state = std::make_shared<SafeDocState>();
	registerSubtree(doc, *state, doc->root, 0);
	std::lock_guard<std::mutex> lock(gRegistryMutex);
	gStates[doc] = std::move(state);
}

void collectSubtree(const coda_doc_t* doc, uint32_t internal, std::vector<uint32_t>& out) {
	if (!doc || internal == 0) return;
	const auto* node = doc->get(internal);
	if (!node || node->kind == coda_doc::Kind::Null) return;
	out.push_back(internal);
	switch (node->kind) {
		case coda_doc::Kind::Block:
		case coda_doc::Kind::KeyedTable:
			for (const auto& [_, child] : node->entries) collectSubtree(doc, child, out);
			break;
		case coda_doc::Kind::Array:
		case coda_doc::Kind::Table:
			for (uint32_t child : node->arr) collectSubtree(doc, child, out);
			break;
		default:
			break;
	}
}

void invalidateNodes(SafeDocState& state, const std::vector<uint32_t>& nodes) {
	for (uint32_t internal : nodes) {
		auto ext = state.internalToExternal.find(internal);
		if (ext != state.internalToExternal.end()) {
			state.externalToInternal.erase(ext->second);
			state.internalToExternal.erase(ext);
		}
		state.parent.erase(internal);
	}
}

bool canAttach(const coda_doc_t* doc, const SafeDocState& state, uint32_t parent, uint32_t child) {
	if (!doc || parent == 0 || child == 0 || child == doc->root) return false;
	auto parentIt = state.parent.find(child);
	if (parentIt == state.parent.end() || parentIt->second != 0) return false;
	for (uint32_t current = parent; current != 0;) {
		if (current == child) return false;
		auto it = state.parent.find(current);
		current = it == state.parent.end() ? 0 : it->second;
	}
	return true;
}

void setInvalidHandleError(coda_error_t* err) {
	if (!err) return;
	coda_error_clear(err);
	err->code = CODA_ERROR_INVALID_HANDLE;
	err->message = owned_from_std("invalid or stale node handle");
}

bool containsColumn(const coda_doc::Node& node, const std::string& column) {
	return std::find(node.cols.begin(), node.cols.end(), column) != node.cols.end();
}

void orderInternal(coda_doc_t& doc, uint32_t internal,
		const std::function<float(const std::string&)>* weightFn) {
	auto* node = doc.get(internal);
	if (!node) return;

	auto sortEntries = [&](bool classifyContainers) {
		std::stable_sort(node->entries.begin(), node->entries.end(),
			[&](const auto& a, const auto& b) {
				if (weightFn) {
					const float wa = (*weightFn)(a.first);
					const float wb = (*weightFn)(b.first);
					if (wa != wb) return wa > wb;
				} else if (classifyContainers) {
					const auto* left = doc.get(a.second);
					const auto* right = doc.get(b.second);
					const bool leftContainer = left && node_is_container(*left);
					const bool rightContainer = right && node_is_container(*right);
					if (leftContainer != rightContainer) return !leftContainer;
				}
				return a.first < b.first;
			});
		rebuild_entry_index(node);
	};

	switch (node->kind) {
		case coda_doc::Kind::Block:
			sortEntries(true);
			for (const auto& [_, child] : node->entries) orderInternal(doc, child, weightFn);
			break;
		case coda_doc::Kind::KeyedTable:
			sortEntries(false);
			break;
		case coda_doc::Kind::Array:
			for (uint32_t child : node->arr) orderInternal(doc, child, weightFn);
			break;
		case coda_doc::Kind::Table:
		case coda_doc::Kind::Row:
		case coda_doc::Kind::String:
		case coda_doc::Kind::Null:
			break;
	}
}

std::shared_ptr<SafeDocState> lockState(
		const coda_doc_t* doc, std::unique_lock<std::mutex>& stateLock) {
	std::lock_guard<std::mutex> registryLock(gRegistryMutex);
	auto it = gStates.find(doc);
	if (it == gStates.end()) return {};
	auto state = it->second;
	stateLock = std::unique_lock<std::mutex>(state->mutex);
	return state;
}

} // namespace

extern "C" CODA_FFI_EXPORT coda_doc_t* coda_doc_new(void) {
	coda_doc_t* doc = coda_unsafe_doc_new();
	registerDocument(doc);
	return doc;
}

extern "C" CODA_FFI_EXPORT void coda_doc_free(coda_doc_t* doc) {
	if (!doc) return;
	std::shared_ptr<SafeDocState> state;
	std::unique_lock<std::mutex> stateLock;
	{
		std::lock_guard<std::mutex> registryLock(gRegistryMutex);
		auto it = gStates.find(doc);
		if (it == gStates.end()) return;
		state = it->second;
		stateLock = std::unique_lock<std::mutex>(state->mutex);
		gStates.erase(it);
	}
	coda_unsafe_doc_free(doc);
}

extern "C" CODA_FFI_EXPORT coda_doc_t* coda_doc_parse(
		const char* src, size_t len, const char* filename, coda_error_t* err) {
	coda_doc_t* doc = coda_unsafe_doc_parse(src, len, filename, err);
	registerDocument(doc);
	return doc;
}

extern "C" CODA_FFI_EXPORT coda_doc_t* coda_doc_parse_file(
		const char* path, coda_error_t* err) {
	coda_doc_t* doc = coda_unsafe_doc_parse_file(path, err);
	registerDocument(doc);
	return doc;
}

extern "C" CODA_FFI_EXPORT coda_doc_t* coda_doc_parse_fp(
		FILE* fp, const char* filename, coda_error_t* err) {
	coda_doc_t* doc = coda_unsafe_doc_parse_fp(fp, filename, err);
	registerDocument(doc);
	return doc;
}

extern "C" CODA_FFI_EXPORT coda_node_t coda_doc_root(const coda_doc_t* doc) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	return doc && state ? wrapInternal(*state, doc->root) : 0;
}

extern "C" CODA_FFI_EXPORT void coda_doc_order(coda_doc_t* doc) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	if (doc && state) orderInternal(*doc, doc->root, nullptr);
}

extern "C" CODA_FFI_EXPORT void coda_doc_order_weighted(
		coda_doc_t* doc, const char** keys, const float* weights, size_t count) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	if (!doc || !state) return;
	std::unordered_map<std::string, float> values;
	for (size_t i = 0; i < count; ++i)
		values[keys && keys[i] ? keys[i] : ""] = weights ? weights[i] : 0.0f;
	const std::function<float(const std::string&)> weightFn = [&](const std::string& key) {
		auto it = values.find(key);
		return it == values.end() ? 0.0f : it->second;
	};
	orderInternal(*doc, doc->root, &weightFn);
}

extern "C" CODA_FFI_EXPORT coda_node_kind_t coda_node_kind(
		const coda_doc_t* doc, coda_node_t node) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	const uint32_t internal = state ? unwrapExternal(*state, node) : 0;
	return internal ? coda_unsafe_node_kind(doc, internal) : CODA_NODE_NULL;
}

extern "C" CODA_FFI_EXPORT int coda_node_is_container(
		const coda_doc_t* doc, coda_node_t node) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	const uint32_t internal = state ? unwrapExternal(*state, node) : 0;
	return internal ? coda_unsafe_node_is_container(doc, internal) : 0;
}

#define CODA_WRAP_NODE_GETTER(name, unsafeName, resultType, fallback) \
extern "C" CODA_FFI_EXPORT resultType name(const coda_doc_t* doc, coda_node_t node) { \
	std::unique_lock<std::mutex> lock; \
	auto state = lockState(doc, lock); \
	const uint32_t internal = state ? unwrapExternal(*state, node) : 0; \
	return internal ? unsafeName(doc, internal) : fallback; \
}

#define CODA_WRAP_NODE_SETTER(name, unsafeName) \
extern "C" CODA_FFI_EXPORT coda_status_t name( \
		coda_doc_t* doc, coda_node_t node, const char* text, size_t len) { \
	std::unique_lock<std::mutex> lock; \
	auto state = lockState(doc, lock); \
	const uint32_t internal = state ? unwrapExternal(*state, node) : 0; \
	return internal ? unsafeName(doc, internal, text, len) : CODA_ERR; \
}

CODA_WRAP_NODE_GETTER(coda_node_comment_get, coda_unsafe_node_comment_get, coda_str_t, view_of(g_empty))
CODA_WRAP_NODE_SETTER(coda_node_comment_set, coda_unsafe_node_comment_set)
CODA_WRAP_NODE_GETTER(coda_node_header_comment_get, coda_unsafe_node_header_comment_get, coda_str_t, view_of(g_empty))
CODA_WRAP_NODE_SETTER(coda_node_header_comment_set, coda_unsafe_node_header_comment_set)
CODA_WRAP_NODE_GETTER(coda_string_get, coda_unsafe_string_get, coda_str_t, view_of(g_empty))
CODA_WRAP_NODE_SETTER(coda_string_set, coda_unsafe_string_set)
CODA_WRAP_NODE_GETTER(coda_array_len, coda_unsafe_array_len, size_t, 0)
CODA_WRAP_NODE_GETTER(coda_map_len, coda_unsafe_map_len, size_t, 0)
CODA_WRAP_NODE_GETTER(coda_table_col_count, coda_unsafe_table_col_count, size_t, 0)
CODA_WRAP_NODE_GETTER(coda_table_row_count, coda_unsafe_table_row_count, size_t, 0)
CODA_WRAP_NODE_GETTER(coda_keyed_table_col_count, coda_unsafe_keyed_table_col_count, size_t, 0)
CODA_WRAP_NODE_GETTER(coda_keyed_table_row_count, coda_unsafe_keyed_table_row_count, size_t, 0)
CODA_WRAP_NODE_GETTER(coda_row_col_count, coda_unsafe_row_col_count, size_t, 0)
CODA_WRAP_NODE_GETTER(coda_row_comment_get, coda_unsafe_row_comment_get, coda_str_t, view_of(g_empty))
CODA_WRAP_NODE_SETTER(coda_row_comment_set, coda_unsafe_row_comment_set)

#undef CODA_WRAP_NODE_GETTER
#undef CODA_WRAP_NODE_SETTER

extern "C" CODA_FFI_EXPORT coda_node_t coda_array_get(
		const coda_doc_t* doc, coda_node_t array, size_t index) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	const uint32_t internal = state ? unwrapExternal(*state, array) : 0;
	return internal ? wrapInternal(*state, coda_unsafe_array_get(doc, internal, index)) : 0;
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_array_push(
		coda_doc_t* doc, coda_node_t array, coda_node_t value) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	const uint32_t a = state ? unwrapExternal(*state, array) : 0;
	const uint32_t v = state ? unwrapExternal(*state, value) : 0;
	if (!a || !v || !canAttach(doc, *state, a, v)) return CODA_ERR;
	const coda_status_t status = coda_unsafe_array_push(doc, a, v);
	if (status == CODA_OK) state->parent[v] = a;
	return status;
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_array_set(
		coda_doc_t* doc, coda_node_t array, size_t index, coda_node_t value) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	const uint32_t a = state ? unwrapExternal(*state, array) : 0;
	const uint32_t v = state ? unwrapExternal(*state, value) : 0;
	if (!a || !v) return CODA_ERR;
	const uint32_t old = coda_unsafe_array_get(doc, a, index);
	if (old == v) return CODA_OK;
	if (!canAttach(doc, *state, a, v)) return CODA_ERR;
	std::vector<uint32_t> removed;
	collectSubtree(doc, old, removed);
	const coda_status_t status = coda_unsafe_array_set(doc, a, index, v);
	if (status == CODA_OK) {
		invalidateNodes(*state, removed);
		state->parent[v] = a;
	}
	return status;
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_array_remove(
		coda_doc_t* doc, coda_node_t array, size_t index) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	const uint32_t a = state ? unwrapExternal(*state, array) : 0;
	if (!a) return CODA_ERR;
	const uint32_t old = coda_unsafe_array_get(doc, a, index);
	std::vector<uint32_t> removed;
	collectSubtree(doc, old, removed);
	const coda_status_t status = coda_unsafe_array_remove(doc, a, index);
	if (status == CODA_OK) invalidateNodes(*state, removed);
	return status;
}

extern "C" CODA_FFI_EXPORT coda_str_t coda_map_key_at(
		const coda_doc_t* doc, coda_node_t map, size_t index) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	const uint32_t internal = state ? unwrapExternal(*state, map) : 0;
	return internal ? coda_unsafe_map_key_at(doc, internal, index) : view_of(g_empty);
}

extern "C" CODA_FFI_EXPORT coda_node_t coda_map_value_at(
		const coda_doc_t* doc, coda_node_t map, size_t index) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	const uint32_t internal = state ? unwrapExternal(*state, map) : 0;
	return internal ? wrapInternal(*state, coda_unsafe_map_value_at(doc, internal, index)) : 0;
}

extern "C" CODA_FFI_EXPORT coda_node_t coda_map_get(
		const coda_doc_t* doc, coda_node_t map, const char* key, size_t keyLen) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	const uint32_t internal = state ? unwrapExternal(*state, map) : 0;
	return internal ? wrapInternal(*state, coda_unsafe_map_get(doc, internal, key, keyLen)) : 0;
}

extern "C" CODA_FFI_EXPORT coda_node_t coda_map_get_or_insert(
		coda_doc_t* doc, coda_node_t map, const char* key, size_t keyLen) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	const uint32_t internal = state ? unwrapExternal(*state, map) : 0;
	if (!internal) return 0;
	const uint32_t child = coda_unsafe_map_get_or_insert(doc, internal, key, keyLen);
	if (child) state->parent[child] = internal;
	return wrapInternal(*state, child);
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_map_set(
		coda_doc_t* doc, coda_node_t map, const char* key, size_t keyLen, coda_node_t value) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	const uint32_t m = state ? unwrapExternal(*state, map) : 0;
	const uint32_t v = state ? unwrapExternal(*state, value) : 0;
	if (!m || !v) return CODA_ERR;
	const uint32_t old = coda_unsafe_map_get(doc, m, key, keyLen);
	if (old == v) return CODA_OK;
	if (!canAttach(doc, *state, m, v)) return CODA_ERR;
	std::vector<uint32_t> removed;
	collectSubtree(doc, old, removed);
	const coda_status_t status = coda_unsafe_map_set(doc, m, key, keyLen, v);
	if (status == CODA_OK) {
		invalidateNodes(*state, removed);
		state->parent[v] = m;
	}
	return status;
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_map_remove(
		coda_doc_t* doc, coda_node_t map, const char* key, size_t keyLen) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	const uint32_t m = state ? unwrapExternal(*state, map) : 0;
	if (!m) return CODA_ERR;
	const uint32_t old = coda_unsafe_map_get(doc, m, key, keyLen);
	std::vector<uint32_t> removed;
	collectSubtree(doc, old, removed);
	const coda_status_t status = coda_unsafe_map_remove(doc, m, key, keyLen);
	if (status == CODA_OK) invalidateNodes(*state, removed);
	return status;
}

#define CODA_WRAP_INDEXED_STR(name, unsafeName) \
extern "C" CODA_FFI_EXPORT coda_str_t name( \
		const coda_doc_t* doc, coda_node_t node, size_t index) { \
	std::unique_lock<std::mutex> lock; \
	auto state = lockState(doc, lock); \
	const uint32_t internal = state ? unwrapExternal(*state, node) : 0; \
	return internal ? unsafeName(doc, internal, index) : view_of(g_empty); \
}

CODA_WRAP_INDEXED_STR(coda_table_col_name, coda_unsafe_table_col_name)
CODA_WRAP_INDEXED_STR(coda_keyed_table_col_name, coda_unsafe_keyed_table_col_name)
CODA_WRAP_INDEXED_STR(coda_keyed_table_row_key_at, coda_unsafe_keyed_table_row_key_at)
CODA_WRAP_INDEXED_STR(coda_row_col_name_at, coda_unsafe_row_col_name_at)
CODA_WRAP_INDEXED_STR(coda_row_col_value_at, coda_unsafe_row_col_value_at)
#undef CODA_WRAP_INDEXED_STR

extern "C" CODA_FFI_EXPORT coda_status_t coda_table_col_append(
		coda_doc_t* doc, coda_node_t table, const char* name, size_t nameLen) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	const uint32_t t = state ? unwrapExternal(*state, table) : 0;
	auto* node = doc && t ? doc->get(t) : nullptr;
	if (!node || node->kind != coda_doc::Kind::Table) return CODA_BAD_KIND;
	const std::string column(name ? name : "", nameLen);
	if (!node->arr.empty() || containsColumn(*node, column)) return CODA_ERR;
	return coda_unsafe_table_col_append(doc, t, name, nameLen);
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_keyed_table_col_append(
		coda_doc_t* doc, coda_node_t table, const char* name, size_t nameLen) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	const uint32_t t = state ? unwrapExternal(*state, table) : 0;
	auto* node = doc && t ? doc->get(t) : nullptr;
	if (!node || node->kind != coda_doc::Kind::KeyedTable) return CODA_BAD_KIND;
	const std::string column(name ? name : "", nameLen);
	if (!node->entries.empty() || containsColumn(*node, column)) return CODA_ERR;
	return coda_unsafe_keyed_table_col_append(doc, t, name, nameLen);
}

extern "C" CODA_FFI_EXPORT coda_node_t coda_table_row_at(
		const coda_doc_t* doc, coda_node_t table, size_t index) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	const uint32_t t = state ? unwrapExternal(*state, table) : 0;
	return t ? wrapInternal(*state, coda_unsafe_table_row_at(doc, t, index)) : 0;
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_table_row_append(
		coda_doc_t* doc, coda_node_t table, coda_node_t row) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	const uint32_t t = state ? unwrapExternal(*state, table) : 0;
	const uint32_t r = state ? unwrapExternal(*state, row) : 0;
	if (!t || !r || !canAttach(doc, *state, t, r)) return CODA_ERR;
	const coda_status_t status = coda_unsafe_table_row_append(doc, t, r);
	if (status == CODA_OK) state->parent[r] = t;
	return status;
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_table_row_set(
		coda_doc_t* doc, coda_node_t table, size_t index, coda_node_t row) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	const uint32_t t = state ? unwrapExternal(*state, table) : 0;
	const uint32_t r = state ? unwrapExternal(*state, row) : 0;
	if (!t || !r) return CODA_ERR;
	const uint32_t old = coda_unsafe_table_row_at(doc, t, index);
	if (old == r) return CODA_OK;
	if (!canAttach(doc, *state, t, r)) return CODA_ERR;
	std::vector<uint32_t> removed;
	collectSubtree(doc, old, removed);
	const coda_status_t status = coda_unsafe_table_row_set(doc, t, index, r);
	if (status == CODA_OK) {
		invalidateNodes(*state, removed);
		state->parent[r] = t;
	}
	return status;
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_table_row_remove(
		coda_doc_t* doc, coda_node_t table, size_t index) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	const uint32_t t = state ? unwrapExternal(*state, table) : 0;
	if (!t) return CODA_ERR;
	const uint32_t old = coda_unsafe_table_row_at(doc, t, index);
	std::vector<uint32_t> removed;
	collectSubtree(doc, old, removed);
	const coda_status_t status = coda_unsafe_table_row_remove(doc, t, index);
	if (status == CODA_OK) invalidateNodes(*state, removed);
	return status;
}

extern "C" CODA_FFI_EXPORT coda_node_t coda_keyed_table_row_at(
		const coda_doc_t* doc, coda_node_t table, size_t index) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	const uint32_t t = state ? unwrapExternal(*state, table) : 0;
	return t ? wrapInternal(*state, coda_unsafe_keyed_table_row_at(doc, t, index)) : 0;
}

extern "C" CODA_FFI_EXPORT coda_node_t coda_keyed_table_row_get(
		const coda_doc_t* doc, coda_node_t table, const char* key, size_t keyLen) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	const uint32_t t = state ? unwrapExternal(*state, table) : 0;
	return t ? wrapInternal(*state, coda_unsafe_keyed_table_row_get(doc, t, key, keyLen)) : 0;
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_keyed_table_row_set(
		coda_doc_t* doc, coda_node_t table, const char* key, size_t keyLen, coda_node_t row) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	const uint32_t t = state ? unwrapExternal(*state, table) : 0;
	const uint32_t r = state ? unwrapExternal(*state, row) : 0;
	if (!t || !r) return CODA_ERR;
	const uint32_t old = coda_unsafe_keyed_table_row_get(doc, t, key, keyLen);
	if (old == r) return CODA_OK;
	if (!canAttach(doc, *state, t, r)) return CODA_ERR;
	std::vector<uint32_t> removed;
	collectSubtree(doc, old, removed);
	const coda_status_t status = coda_unsafe_keyed_table_row_set(doc, t, key, keyLen, r);
	if (status == CODA_OK) {
		invalidateNodes(*state, removed);
		state->parent[r] = t;
	}
	return status;
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_keyed_table_row_remove(
		coda_doc_t* doc, coda_node_t table, const char* key, size_t keyLen) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	const uint32_t t = state ? unwrapExternal(*state, table) : 0;
	if (!t) return CODA_ERR;
	const uint32_t old = coda_unsafe_keyed_table_row_get(doc, t, key, keyLen);
	std::vector<uint32_t> removed;
	collectSubtree(doc, old, removed);
	const coda_status_t status = coda_unsafe_keyed_table_row_remove(doc, t, key, keyLen);
	if (status == CODA_OK) invalidateNodes(*state, removed);
	return status;
}

extern "C" CODA_FFI_EXPORT coda_str_t coda_row_get(
		const coda_doc_t* doc, coda_node_t row, const char* column, size_t columnLen) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	const uint32_t r = state ? unwrapExternal(*state, row) : 0;
	return r ? coda_unsafe_row_get(doc, r, column, columnLen) : view_of(g_empty);
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_row_set(
		coda_doc_t* doc, coda_node_t row, const char* column, size_t columnLen,
		const char* value, size_t valueLen) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	const uint32_t r = state ? unwrapExternal(*state, row) : 0;
	auto* rowNode = doc && r ? doc->get(r) : nullptr;
	if (!rowNode || rowNode->kind != coda_doc::Kind::Row) return CODA_BAD_KIND;
	const uint32_t parent = state->parent.at(r);
	if (parent != 0) {
		const auto* parentNode = doc->get(parent);
		const std::string name(column ? column : "", columnLen);
		if (!parentNode ||
			(parentNode->kind != coda_doc::Kind::Table && parentNode->kind != coda_doc::Kind::KeyedTable) ||
			!containsColumn(*parentNode, name) || rowNode->field_index.find(name) == rowNode->field_index.end())
			return CODA_ERR;
	}
	return coda_unsafe_row_set(doc, r, column, columnLen, value, valueLen);
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_row_remove(
		coda_doc_t* doc, coda_node_t row, const char* column, size_t columnLen) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	const uint32_t r = state ? unwrapExternal(*state, row) : 0;
	if (!r) return CODA_ERR;
	if (state->parent.at(r) != 0) return CODA_ERR;
	return coda_unsafe_row_remove(doc, r, column, columnLen);
}

extern "C" CODA_FFI_EXPORT coda_owned_str_t coda_node_serialize(
		const coda_doc_t* doc, coda_node_t node, const char* indent, size_t indentLen,
		coda_error_t* err) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	const uint32_t internal = state ? unwrapExternal(*state, node) : 0;
	if (!internal) {
		setInvalidHandleError(err);
		return {nullptr, 0};
	}
	return coda_unsafe_node_serialize(doc, internal, indent, indentLen, err);
}

extern "C" CODA_FFI_EXPORT void coda_node_order(coda_doc_t* doc, coda_node_t node) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	const uint32_t internal = state ? unwrapExternal(*state, node) : 0;
	if (internal) orderInternal(*doc, internal, nullptr);
}

extern "C" CODA_FFI_EXPORT void coda_node_order_weighted(
		coda_doc_t* doc, coda_node_t node, const char** keys, const float* weights,
		size_t count) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	const uint32_t internal = state ? unwrapExternal(*state, node) : 0;
	if (!internal) return;
	std::unordered_map<std::string, float> values;
	for (size_t i = 0; i < count; ++i)
		values[keys && keys[i] ? keys[i] : ""] = weights ? weights[i] : 0.0f;
	const std::function<float(const std::string&)> weightFn = [&](const std::string& key) {
		auto it = values.find(key);
		return it == values.end() ? 0.0f : it->second;
	};
	orderInternal(*doc, internal, &weightFn);
}

#define CODA_WRAP_NEW_NODE(name, unsafeName) \
extern "C" CODA_FFI_EXPORT coda_node_t name(coda_doc_t* doc) { \
	std::unique_lock<std::mutex> lock; \
	auto state = lockState(doc, lock); \
	return state ? wrapInternal(*state, unsafeName(doc)) : 0; \
}

extern "C" CODA_FFI_EXPORT coda_node_t coda_new_string(
		coda_doc_t* doc, const char* value, size_t valueLen) {
	std::unique_lock<std::mutex> lock;
	auto state = lockState(doc, lock);
	return state ? wrapInternal(*state, coda_unsafe_new_string(doc, value, valueLen)) : 0;
}

CODA_WRAP_NEW_NODE(coda_new_block, coda_unsafe_new_block)
CODA_WRAP_NEW_NODE(coda_new_array, coda_unsafe_new_array)
CODA_WRAP_NEW_NODE(coda_new_table, coda_unsafe_new_table)
CODA_WRAP_NEW_NODE(coda_new_keyed_table, coda_unsafe_new_keyed_table)
CODA_WRAP_NEW_NODE(coda_new_row, coda_unsafe_new_row)
#undef CODA_WRAP_NEW_NODE
