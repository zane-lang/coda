#pragma once

#include <type_traits>
#include <utility>
#include <variant>

namespace coda {

// Helper for overloading lambdas (the classic std::visit overload set).
template<typename... Ts>
struct overloaded : Ts... {
	using Ts::operator()...;
};

template<typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

// A thin, ergonomic wrapper around std::variant offering visit()/match().
// (Deliberately small: no speculative serialization framework.)
template<typename... Types>
struct Variant {
	std::variant<Types...> value;

	Variant() = default;
	Variant(std::variant<Types...> value)
	: value(std::move(value)) {}

	template<typename T,
	typename = std::enable_if_t<(std::is_same_v<std::decay_t<T>, Types> || ...)>>
	Variant(T&& val) : value(std::forward<T>(val)) {}

	template<typename T,
	typename = std::enable_if_t<(std::is_same_v<std::decay_t<T>, Types> || ...)>>
	Variant& operator=(T&& val) {
		value = std::forward<T>(val);
		return *this;
	}

	template<typename Callback>
	decltype(auto) visit(Callback&& callback) {
		return std::visit(std::forward<Callback>(callback), value);
	}

	template<typename Callback>
	decltype(auto) visit(Callback&& callback) const {
		return std::visit(std::forward<Callback>(callback), value);
	}

	template<typename... Callbacks>
	decltype(auto) match(Callbacks&&... callbacks) {
		return std::visit(overloaded{std::forward<Callbacks>(callbacks)...}, value);
	}

	template<typename... Callbacks>
	decltype(auto) match(Callbacks&&... callbacks) const {
		return std::visit(overloaded{std::forward<Callbacks>(callbacks)...}, value);
	}
};

} // namespace coda
