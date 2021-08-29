#include <memory>
#include <gtest/gtest.h>
#include <entt/core/memory.hpp>

struct test_allocator: std::allocator<int> {
    using base = std::allocator<int>;
    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;

    using std::allocator<int>::allocator;

    test_allocator & operator=(const test_allocator &other) {
        // necessary to avoid call suppression
        base::operator=(other);
        return *this;
    }
};

TEST(Memory, ToAddress) {
    std::shared_ptr<int> shared = std::make_shared<int>();
    auto *plain = std::addressof(*shared);

    ASSERT_EQ(entt::to_address(shared), plain);
    ASSERT_EQ(entt::to_address(plain), plain);
}

TEST(Memory, PoccaPocmaAndPocs) {
    test_allocator lhs, rhs;
    // honestly, I don't even know how one is supposed to test such a thing :)
    entt::propagate_on_container_copy_assignment(lhs, rhs);
    entt::propagate_on_container_move_assignment(lhs, rhs);
    entt::propagate_on_container_swap(lhs, rhs);
}

TEST(Memory, IsPowerOfTwo) {
    ASSERT_FALSE(entt::is_power_of_two_v<0u>);
    ASSERT_TRUE(entt::is_power_of_two_v<1u>);
    ASSERT_TRUE(entt::is_power_of_two_v<2u>);
    ASSERT_TRUE(entt::is_power_of_two_v<4u>);
    ASSERT_FALSE(entt::is_power_of_two_v<7u>);
    ASSERT_TRUE(entt::is_power_of_two_v<128u>);
    ASSERT_FALSE(entt::is_power_of_two_v<200u>);
}
