#pragma once

#include <variant>

// Helper for overloading lambdas
template<typename... Ts>
struct overloaded : Ts... {
	using Ts::operator()...;
};

template<typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

// Helper function to load a variant at a specific index
template<std::size_t I = 0, typename... Types, typename Archive>
void loadVariantAt(std::size_t idx, std::variant<Types...>& var, Archive& ar) {
	if constexpr (I<sizeof...(Types)) {
		if (I == idx) {
			std::variant_alternative_t<I, std::variant<Types...>> val;
			ar(val);
			var = std::move(val);
		} else {
			loadVariantAt<I + 1>(idx, var, ar);
		}
	}
}

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

	// Match with callbacks - automatically adds a no-op catch-all if needed
	template<typename... Callbacks>
	decltype(auto) match(Callbacks&&... callbacks) {
		return std::visit(overloaded{std::forward<Callbacks>(callbacks)..., [](auto&&){}}, value);
	}

	template<typename... Callbacks>
	decltype(auto) match(Callbacks&&... callbacks) const {
		return std::visit(overloaded{std::forward<Callbacks>(callbacks)..., [](auto&&){}}, value);
	}

	template<typename Archive>
	void save(Archive& ar) const {
		ar(value.index());
		std::visit([&ar](const auto& v) { ar(v); }, value);
	}

	template<typename Archive>
	void load(Archive& ar) {
		std::size_t idx;
		ar(idx);
		loadVariantAt<0>(idx, value, ar);
	}
};

template<template<typename> class Wrapper, typename... Types>
struct WrappingVariant {
	std::variant<Wrapper<Types>...> value;

	WrappingVariant() = default;
	WrappingVariant(std::variant<Types...> value)
	: value(std::move(value)) {}

	template<typename T,
	typename = std::enable_if_t<(std::is_same_v<std::decay_t<T>, Types> || ...)>>
	WrappingVariant(T&& val) : value(std::forward<T>(val)) {}

	template<typename T,
	typename = std::enable_if_t<(std::is_same_v<std::decay_t<T>, Types> || ...)>>
	WrappingVariant& operator=(T&& val) {
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

	// Match with callbacks - automatically adds a no-op catch-all if needed
	template<typename... Callbacks>
	decltype(auto) match(Callbacks&&... callbacks) {
		return std::visit(overloaded{std::forward<Callbacks>(callbacks)..., [](auto&&){}}, value);
	}

	template<typename... Callbacks>
	decltype(auto) match(Callbacks&&... callbacks) const {
		return std::visit(overloaded{std::forward<Callbacks>(callbacks)..., [](auto&&){}}, value);
	}

	template<typename Archive>
	void save(Archive& ar) const {
		ar(value.index());
		std::visit([&ar](const auto& v) { ar(v); }, value);
	}

	template<typename Archive>
	void load(Archive& ar) {
		std::size_t idx;
		ar(idx);
		loadVariantAt<0>(idx, value, ar);
	}
};
