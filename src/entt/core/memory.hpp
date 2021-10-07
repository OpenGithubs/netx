#ifndef ENTT_CORE_MEMORY_HPP
#define ENTT_CORE_MEMORY_HPP

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>
#include "../config/config.h"

namespace entt {

/**
 * @brief Unwraps fancy pointers, does nothing otherwise (waiting for C++20).
 * @tparam Type Pointer type.
 * @param ptr Fancy or raw pointer.
 * @return A raw pointer that represents the address of the original pointer.
 */
template<typename Type>
[[nodiscard]] constexpr auto to_address(Type &&ptr) ENTT_NOEXCEPT {
    if constexpr(std::is_pointer_v<std::remove_const_t<std::remove_reference_t<Type>>>) {
        return ptr;
    } else {
        return to_address(std::forward<Type>(ptr).operator->());
    }
}

/**
 * @brief Utility function to design allocation-aware containers.
 * @tparam Allocator Type of allocator.
 * @param lhs A valid allocator.
 * @param rhs Another valid allocator.
 */
template<typename Allocator>
constexpr void propagate_on_container_copy_assignment([[maybe_unused]] Allocator &lhs, [[maybe_unused]] Allocator &rhs) ENTT_NOEXCEPT {
    if constexpr(std::allocator_traits<Allocator>::propagate_on_container_copy_assignment::value) {
        lhs = rhs;
    }
}

/**
 * @brief Utility function to design allocation-aware containers.
 * @tparam Allocator Type of allocator.
 * @param lhs A valid allocator.
 * @param rhs Another valid allocator.
 */
template<typename Allocator>
constexpr void propagate_on_container_move_assignment([[maybe_unused]] Allocator &lhs, [[maybe_unused]] Allocator &rhs) ENTT_NOEXCEPT {
    if constexpr(std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value) {
        lhs = std::move(rhs);
    }
}

/**
 * @brief Utility function to design allocation-aware containers.
 * @tparam Allocator Type of allocator.
 * @param lhs A valid allocator.
 * @param rhs Another valid allocator.
 */
template<typename Allocator>
constexpr void propagate_on_container_swap([[maybe_unused]] Allocator &lhs, [[maybe_unused]] Allocator &rhs) ENTT_NOEXCEPT {
    ENTT_ASSERT(std::allocator_traits<Allocator>::propagate_on_container_swap::value || lhs == rhs, "Cannot swap the containers");

    if constexpr(std::allocator_traits<Allocator>::propagate_on_container_swap::value) {
        using std::swap;
        swap(lhs, rhs);
    }
}

/**
 * @brief Checks whether a value is a power of two or not.
 * @param value A value that may or may not be a power of two.
 * @return True if the value is a power of two, false otherwise.
 */
[[nodiscard]] inline constexpr bool is_power_of_two(const std::size_t value) ENTT_NOEXCEPT {
    return value && ((value & (value - 1)) == 0);
}

/**
 * @brief Computes the smallest power of two greater than or equal to a value.
 * @param value The value to use.
 * @return The smallest power of two greater than or equal to the given value.
 */
[[nodiscard]] inline constexpr std::size_t next_power_of_two(const std::size_t value) ENTT_NOEXCEPT {
    std::size_t curr = value;

    curr |= curr >> 1;
    curr |= curr >> 2;
    curr |= curr >> 4;
    curr |= curr >> 8;
    curr |= curr >> 16;

    if constexpr(sizeof(value) > sizeof(std::uint32_t)) {
        curr |= curr >> 32;
    }

    return ++curr;
}

/**
 * @brief Fast module utility function (powers of two only).
 * @tparam Value Compile-time page size, it must be a power of two.
 * @param value A value for which to calculate the modulus.
 * @return Remainder of division.
 */
template<std::size_t Value>
[[nodiscard]] constexpr std::size_t fast_mod(const std::size_t value) ENTT_NOEXCEPT {
    static_assert(is_power_of_two(Value), "Value must be a power of two");
    return value & (Value - 1u);
}

} // namespace entt

#endif
