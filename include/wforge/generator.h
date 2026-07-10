#ifndef WFORGE_GENERATOR_H
#define WFORGE_GENERATOR_H

#include <coroutine>
#include <exception>
#include <iterator>
#include <utility>

namespace wf {

template<typename T>
class Generator {
public:
	struct promise_type;

private:
	using handle_type = std::coroutine_handle<promise_type>;

public:
	struct promise_type {
		T current_value;

		auto get_return_object() {
			return Generator{handle_type::from_promise(*this)};
		}

		auto initial_suspend() noexcept {
			return std::suspend_always{};
		}

		auto final_suspend() noexcept {
			return std::suspend_always{};
		}

		auto yield_value(T value) noexcept {
			current_value = std::move(value);
			return std::suspend_always{};
		}

		void return_void() noexcept {}

		void unhandled_exception() {
			std::terminate();
		}
	};

	Generator(handle_type h) noexcept: _handle(h) {}

	Generator(Generator &&other) noexcept
		: _handle(std::exchange(other._handle, nullptr)) {}

	Generator &operator=(Generator &&other) noexcept {
		if (this != &other) {
			if (_handle) {
				_handle.destroy();
			}
			_handle = std::exchange(other._handle, nullptr);
		}
		return *this;
	}

	Generator(const Generator &) = delete;
	Generator &operator=(const Generator &) = delete;

	~Generator() {
		if (_handle) {
			_handle.destroy();
		}
	}

	struct iterator {
		handle_type _handle;

		iterator &operator++() {
			_handle.resume();
			return *this;
		}

		const T &operator*() const noexcept {
			return _handle.promise().current_value;
		}

		bool operator==(std::default_sentinel_t) const {
			return !_handle || _handle.done();
		}
	};

	iterator begin() {
		_handle.resume();
		return {_handle};
	}

	std::default_sentinel_t end() noexcept {
		return std::default_sentinel;
	}

private:
	handle_type _handle;
};

} // namespace wf

#endif // WFORGE_GENERATOR_H
