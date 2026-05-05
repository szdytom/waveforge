#ifndef WFORGE_CTTI_H
#define WFORGE_CTTI_H

#include <string_view>

namespace wf {

constexpr std::size_t fnv1a(std::string_view str) noexcept {
	std::size_t hash = 0xcbf29ce484222325;
	for (char c : str) {
		hash ^= static_cast<std::size_t>(c);
		hash *= 0x100000001b3;
	}
	return hash;
}

template<typename T>
constexpr std::size_t typeHash() noexcept {
	constexpr std::string_view name =
#ifdef _MSC_VER
		__FUNCSIG__;
#else
		__PRETTY_FUNCTION__;
#endif

	// Since the prefix and suffix are fixed, we don't need to extract the
	// actual type name, we can just hash the whole string FNV-1a 64-bit hash
	return fnv1a(name);
}

} // namespace wf

#endif // WFORGE_CTTI_H
