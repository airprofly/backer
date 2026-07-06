#pragma once

#include <type_traits>
#include <utility>

namespace backer {

/// Minimal `Expected<T, E>` polyfill for C++17.
/// Provides a subset of std::expected (C++23) sufficient for this project.
template <typename T, typename E = ErrorCode>
class Expected {
    static_assert(!std::is_same_v<T, void>,
                  "Use the void specialization for Expected<void, E>");

    union {
        T value_;
        E error_;
    };
    bool hasValue_;

    void destroy() noexcept {
        if (hasValue_) {
            value_.~T();
        } else {
            error_.~E();
        }
    }

public:
    Expected(T const& val) noexcept(std::is_nothrow_copy_constructible_v<T>)
        : value_(val), hasValue_(true) {}

    Expected(T&& val) noexcept(std::is_nothrow_move_constructible_v<T>)
        : value_(std::move(val)), hasValue_(true) {}

    Expected(E const& err) noexcept(std::is_nothrow_copy_constructible_v<E>)
        : error_(err), hasValue_(false) {}

    Expected(Expected const& other) noexcept(
        std::is_nothrow_copy_constructible_v<T> &&
        std::is_nothrow_copy_constructible_v<E>)
        : hasValue_(other.hasValue_)
    {
        if (hasValue_) {
            ::new (&value_) T(other.value_);
        } else {
            ::new (&error_) E(other.error_);
        }
    }

    Expected(Expected&& other) noexcept(
        std::is_nothrow_move_constructible_v<T> &&
        std::is_nothrow_move_constructible_v<E>)
        : hasValue_(other.hasValue_)
    {
        if (hasValue_) {
            ::new (&value_) T(std::move(other.value_));
        } else {
            ::new (&error_) E(std::move(other.error_));
        }
    }

    ~Expected() noexcept { destroy(); }

    explicit operator bool() const noexcept { return hasValue_; }
    bool has_value() const noexcept { return hasValue_; }

    T& value() & noexcept { return value_; }
    T const& value() const& noexcept { return value_; }
    T&& value() && noexcept { return std::move(value_); }

    E& error() & noexcept { return error_; }
    E const& error() const& noexcept { return error_; }

    /// Return value if present, otherwise a default.
    T value_or(T defaultVal) const& noexcept(std::is_nothrow_move_constructible_v<T>) {
        return hasValue_ ? value_ : std::move(defaultVal);
    }
};

// ── void specialization ───────────────────────────────────────────────────

template <typename E>
class Expected<void, E> {
    union { E error_; };
    bool hasValue_;

    void destroy() noexcept {
        if (!hasValue_) {
            error_.~E();
        }
    }

public:
    Expected() noexcept : hasValue_(true) {}
    Expected(E const& err) noexcept : error_(err), hasValue_(false) {}

    Expected(Expected const& other) noexcept(std::is_nothrow_copy_constructible_v<E>)
        : hasValue_(other.hasValue_)
    {
        if (!hasValue_) {
            ::new (&error_) E(other.error_);
        }
    }

    Expected(Expected&& other) noexcept(std::is_nothrow_move_constructible_v<E>)
        : hasValue_(other.hasValue_)
    {
        if (!hasValue_) {
            ::new (&error_) E(std::move(other.error_));
        }
    }

    ~Expected() noexcept { destroy(); }

    explicit operator bool() const noexcept { return hasValue_; }
    bool has_value() const noexcept { return hasValue_; }
    void value() const noexcept {}

    E& error() & noexcept { return error_; }
    E const& error() const& noexcept { return error_; }
};

} // namespace backer
