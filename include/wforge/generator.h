#ifndef WFORGE_GENERATOR_H
#define WFORGE_GENERATOR_H

// Reference: lewissbaker/generator (P2168 std::generator reference
// implementation) Boost Software License 1.0

#include <cassert>
#include <coroutine>
#include <exception>
#include <iterator>
#include <type_traits>
#include <utility>

namespace wf {

template<typename T>
union manual_lifetime {
	manual_lifetime() noexcept {}
	~manual_lifetime() {}

	template<typename... Args>
	T &construct(
		Args &&...args
	) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
		return *::new (static_cast<void *>(&value)) T((Args &&)args...);
	}

	void destruct() noexcept(std::is_nothrow_destructible_v<T>) {
		value.~T();
	}

	T &get() noexcept {
		return value;
	}

	const T &get() const noexcept {
		return value;
	}

	T value;
};

template<typename Ref, typename Value = std::remove_cvref_t<Ref>>
class Generator;

template<typename Ref, typename Value>
class Generator {
public:
	using reference = Ref;
	using value_type = Value;

	struct promise_type;

private:
	using handle_type = std::coroutine_handle<promise_type>;

public:
	struct promise_type {
		manual_lifetime<Ref> current_value;
		manual_lifetime<std::exception_ptr> exception;

		Generator get_return_object() noexcept {
			return Generator{handle_type::from_promise(*this)};
		}

		std::suspend_always initial_suspend() noexcept {
			return {};
		}

		auto final_suspend() noexcept;

		void return_void() noexcept {}

		void unhandled_exception() {
			exception.construct(std::current_exception());
		}

		template<typename U>
		requires std::is_convertible_v<U &&, Ref>
		std::suspend_always yield_value(
			U &&value
		) noexcept(std::is_nothrow_constructible_v<Ref, U>) {
			current_value.construct((U &&)value);
			return {};
		}

		std::suspend_always yield_value(
			Value &&value
		) noexcept(std::is_nothrow_move_constructible_v<Value>) {
			current_value.construct(static_cast<Value &&>(value));
			return {};
		}

		void await_transform() = delete;
	};

	struct final_awaiter {
		bool await_ready() noexcept {
			return false;
		}

		std::coroutine_handle<> await_suspend(handle_type h) noexcept {
			return std::noop_coroutine();
		}

		void await_resume() noexcept {}
	};

	Generator() noexcept: _handle(nullptr), _started(false) {}

	Generator(Generator &&other) noexcept
		: _handle(std::exchange(other._handle, nullptr))
		, _started(std::exchange(other._started, false)) {}

	Generator &operator=(Generator &&other) noexcept {
		if (this != &other) {
			if (_handle) {
				if (_started && !_handle.done()) {
					_handle.promise().current_value.destruct();
				}
				_handle.destroy();
			}
			_handle = std::exchange(other._handle, nullptr);
			_started = std::exchange(other._started, false);
		}
		return *this;
	}

	Generator(const Generator &) = delete;
	Generator &operator=(const Generator &) = delete;

	~Generator() {
		if (_handle) {
			if (_started && !_handle.done()) {
				_handle.promise().current_value.destruct();
			}
			_handle.destroy();
		}
	}

	struct sentinel {};

	struct iterator {
		using iterator_category = std::input_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = Value;
		using reference = Ref;
		using pointer = std::add_pointer_t<Ref>;

		iterator() noexcept: _handle(nullptr) {}
		iterator(const iterator &) = delete;

		iterator(iterator &&other) noexcept
			: _handle(std::exchange(other._handle, nullptr)) {}

		iterator &operator=(iterator &&other) noexcept {
			std::swap(_handle, other._handle);
			return *this;
		}

		~iterator() = default;

		bool operator==(sentinel) const noexcept {
			return _handle.done();
		}

		iterator &operator++() {
			auto &promise = _handle.promise();
			promise.current_value.destruct();
			if (promise.exception.get()) {
				std::rethrow_exception(std::move(promise.exception.get()));
			}
			_handle.resume();
			return *this;
		}

		void operator++(int) {
			(void)operator++();
		}

		reference operator*() const noexcept {
			return static_cast<reference>(
				_handle.promise().current_value.get()
			);
		}

	private:
		friend class Generator;

		explicit iterator(handle_type h) noexcept: _handle(h) {}

		handle_type _handle;
	};

	iterator begin() {
		assert(_handle);
		assert(!_started);
		_started = true;
		_handle.resume();
		if (_handle.promise().exception.get()) {
			std::rethrow_exception(
				std::move(_handle.promise().exception.get())
			);
		}
		return iterator{_handle};
	}

	sentinel end() noexcept {
		return {};
	}

private:
	explicit Generator(handle_type h) noexcept: _handle(h), _started(false) {}

	handle_type _handle;
	bool _started = false;
};

template<typename Ref, typename Value>
auto Generator<Ref, Value>::promise_type::final_suspend() noexcept {
	return final_awaiter{};
}

} // namespace wf

#endif // WFORGE_GENERATOR_H
