#ifndef WFORGE_CTTI_H
#define WFORGE_CTTI_H

#include <string_view>

namespace wf {

constexpr std::size_t fnv1a(const char *str) noexcept {
	std::size_t hash = 0xcbf29ce484222325;
	while (*str) {
		hash ^= static_cast<std::size_t>(*str++);
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

	return fnv1a(name.data());
}

} // namespace wf

#endif // WFORGE_CTTI_H
