#pragma once

#include <cstddef>

namespace backer {

/// Minimal `span` polyfill for C++17 (subset of std::span from C++20).
/// Provides a non-owning view over a contiguous sequence of bytes.
template <typename T>
class span {
public:
    using pointer         = T*;
    using const_iterator  = T const*;
    using size_type       = std::size_t;

    constexpr span() noexcept = default;

    constexpr span(T* data, size_type count) noexcept
        : data_(data), size_(count) {}

    template <typename U, std::size_t N>
    constexpr span(U (&arr)[N]) noexcept
        : data_(arr), size_(N) {}

    template <typename Cont>
    constexpr span(Cont const& c) noexcept
        : data_(c.data()), size_(c.size()) {}

    constexpr const_iterator data() const noexcept { return data_; }
    constexpr pointer         data() noexcept       { return data_; }
    constexpr size_type       size() const noexcept  { return size_; }
    constexpr bool            empty() const noexcept { return size_ == 0; }

    constexpr const_iterator begin() const noexcept { return data_; }
    constexpr const_iterator end()   const noexcept { return data_ + size_; }

private:
    T*       data_ = nullptr;
    size_type size_ = 0;
};

} // namespace backer
