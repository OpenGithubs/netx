#include <algorithm>
#include <functional>
#include <iterator>
#include <utility>
#include <gtest/gtest.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/sparse_set.hpp>
#include "../common/config.h"
#include "../common/throwing_allocator.hpp"

static auto testing_values() {
    return testing::Values(entt::deletion_policy::swap_and_pop, entt::deletion_policy::in_place, entt::deletion_policy::swap_only);
}

static auto policy_to_string(const testing::TestParamInfo<entt::deletion_policy> &info) {
    const char *ret = nullptr;

    switch(info.param) {
    case entt::deletion_policy::swap_and_pop:
        ret = "SwapAndPop";
        break;
    case entt::deletion_policy::in_place:
        ret = "InPlace";
        break;
    case entt::deletion_policy::swap_only:
        ret = "SwapOnly";
        break;
    }

    return ret;
}

class Policy: public testing::TestWithParam<entt::deletion_policy> {};
class PolicyDeathTest: public testing::TestWithParam<entt::deletion_policy> {};

INSTANTIATE_TEST_SUITE_P(All, Policy, testing_values(), policy_to_string);
INSTANTIATE_TEST_SUITE_P(All, PolicyDeathTest, testing_values(), policy_to_string);

TEST_P(Policy, Constructors) {
    entt::sparse_set set{};

    ASSERT_EQ(set.policy(), entt::deletion_policy::swap_and_pop);
    ASSERT_NO_FATAL_FAILURE([[maybe_unused]] auto alloc = set.get_allocator());
    ASSERT_EQ(set.type(), entt::type_id<void>());

    set = entt::sparse_set{std::allocator<entt::entity>{}};

    ASSERT_EQ(set.policy(), entt::deletion_policy::swap_and_pop);
    ASSERT_NO_FATAL_FAILURE([[maybe_unused]] auto alloc = set.get_allocator());
    ASSERT_EQ(set.type(), entt::type_id<void>());

    set = entt::sparse_set{GetParam(), std::allocator<entt::entity>{}};

    ASSERT_EQ(set.policy(), GetParam());
    ASSERT_NO_FATAL_FAILURE([[maybe_unused]] auto alloc = set.get_allocator());
    ASSERT_EQ(set.type(), entt::type_id<void>());

    set = entt::sparse_set{entt::type_id<int>(), GetParam(), std::allocator<entt::entity>{}};

    ASSERT_EQ(set.policy(), GetParam());
    ASSERT_NO_FATAL_FAILURE([[maybe_unused]] auto alloc = set.get_allocator());
    ASSERT_EQ(set.type(), entt::type_id<int>());
}

TEST_P(Policy, Move) {
    entt::sparse_set set{GetParam()};

    set.push(entt::entity{42});

    ASSERT_TRUE(std::is_move_constructible_v<decltype(set)>);
    ASSERT_TRUE(std::is_move_assignable_v<decltype(set)>);

    entt::sparse_set other{std::move(set)};

    ASSERT_TRUE(set.empty());
    ASSERT_FALSE(other.empty());

    ASSERT_EQ(set.policy(), GetParam());
    ASSERT_EQ(other.policy(), GetParam());

    ASSERT_EQ(set.at(0u), static_cast<entt::entity>(entt::null));
    ASSERT_EQ(other.at(0u), entt::entity{42});

    entt::sparse_set extended{std::move(other), std::allocator<entt::entity>{}};

    ASSERT_TRUE(other.empty());
    ASSERT_FALSE(extended.empty());

    ASSERT_EQ(other.policy(), GetParam());
    ASSERT_EQ(extended.policy(), GetParam());

    ASSERT_EQ(other.at(0u), static_cast<entt::entity>(entt::null));
    ASSERT_EQ(extended.at(0u), entt::entity{42});

    set = std::move(extended);

    ASSERT_FALSE(set.empty());
    ASSERT_TRUE(other.empty());
    ASSERT_TRUE(extended.empty());

    ASSERT_EQ(set.policy(), GetParam());
    ASSERT_EQ(other.policy(), GetParam());
    ASSERT_EQ(extended.policy(), GetParam());

    ASSERT_EQ(set.at(0u), entt::entity{42});
    ASSERT_EQ(other.at(0u), static_cast<entt::entity>(entt::null));
    ASSERT_EQ(extended.at(0u), static_cast<entt::entity>(entt::null));

    other = entt::sparse_set{GetParam()};
    other.push(entt::entity{3});
    other = std::move(set);

    ASSERT_TRUE(set.empty());
    ASSERT_FALSE(other.empty());

    ASSERT_EQ(set.policy(), GetParam());
    ASSERT_EQ(other.policy(), GetParam());

    ASSERT_EQ(set.at(0u), static_cast<entt::entity>(entt::null));
    ASSERT_EQ(other.at(0u), entt::entity{42});
}

TEST_P(Policy, Swap) {
    entt::sparse_set set{GetParam()};
    entt::sparse_set other{entt::deletion_policy::in_place};

    ASSERT_EQ(set.policy(), GetParam());
    ASSERT_EQ(other.policy(), entt::deletion_policy::in_place);

    set.push(entt::entity{42});

    other.push(entt::entity{9});
    other.push(entt::entity{3});
    other.erase(entt::entity{9});

    ASSERT_EQ(set.size(), 1u);
    ASSERT_EQ(other.size(), 2u);

    set.swap(other);

    ASSERT_EQ(set.policy(), entt::deletion_policy::in_place);
    ASSERT_EQ(other.policy(), GetParam());

    ASSERT_EQ(set.size(), 2u);
    ASSERT_EQ(other.size(), 1u);

    ASSERT_EQ(set.at(1u), entt::entity{3});
    ASSERT_EQ(other.at(0u), entt::entity{42});
}

TEST(SwapAndPop, FreeList) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{entt::deletion_policy::swap_and_pop};

    ASSERT_EQ(set.size(), 0u);
    ASSERT_EQ(set.free_list(), traits_type::to_entity(entt::tombstone));

    set.push(entt::entity{3});
    set.push(entt::entity{42});
    set.erase(entt::entity{3});

    ASSERT_EQ(set.size(), 1u);
    ASSERT_EQ(set.free_list(), traits_type::to_entity(entt::tombstone));

    set.clear();

    ASSERT_EQ(set.size(), 0u);
    ASSERT_EQ(set.free_list(), traits_type::to_entity(entt::tombstone));
}

ENTT_DEBUG_TEST(SwapAndPopDeathTest, FreeList) {
    entt::sparse_set set{entt::deletion_policy::swap_and_pop};

    set.push(entt::entity{3});

    ASSERT_DEATH(set.free_list(0u), "");
    ASSERT_DEATH(set.free_list(1u), "");
    ASSERT_DEATH(set.free_list(2u), "");
}

TEST(InPlace, FreeList) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{entt::deletion_policy::in_place};

    ASSERT_EQ(set.size(), 0u);
    ASSERT_EQ(set.free_list(), traits_type::to_entity(entt::tombstone));

    set.push(entt::entity{3});
    set.push(entt::entity{42});
    set.erase(entt::entity{3});

    ASSERT_EQ(set.size(), 2u);
    ASSERT_EQ(set.free_list(), 0u);

    set.clear();

    ASSERT_EQ(set.size(), 0u);
    ASSERT_EQ(set.free_list(), traits_type::to_entity(entt::tombstone));
}

ENTT_DEBUG_TEST(InPlaceDeathTest, FreeList) {
    entt::sparse_set set{entt::deletion_policy::in_place};

    set.push(entt::entity{3});

    ASSERT_DEATH(set.free_list(0u), "");
    ASSERT_DEATH(set.free_list(1u), "");
    ASSERT_DEATH(set.free_list(2u), "");
}

TEST(SwapOnly, FreeList) {
    entt::sparse_set set{entt::deletion_policy::swap_only};

    ASSERT_EQ(set.size(), 0u);
    ASSERT_EQ(set.free_list(), 0u);

    set.push(entt::entity{3});
    set.push(entt::entity{42});
    set.erase(entt::entity{3});

    ASSERT_EQ(set.size(), 2u);
    ASSERT_EQ(set.free_list(), 1u);

    set.free_list(0u);

    ASSERT_EQ(set.size(), 2u);
    ASSERT_EQ(set.free_list(), 0u);

    set.free_list(2u);

    ASSERT_EQ(set.size(), 2u);
    ASSERT_EQ(set.free_list(), 2u);

    set.clear();

    ASSERT_EQ(set.size(), 0u);
    ASSERT_EQ(set.free_list(), 0u);
}

ENTT_DEBUG_TEST(SwapOnlyDeathTest, FreeList) {
    entt::sparse_set set{entt::deletion_policy::swap_only};

    set.push(entt::entity{3});

    ASSERT_NO_FATAL_FAILURE(set.free_list(0u));
    ASSERT_NO_FATAL_FAILURE(set.free_list(1u));
    ASSERT_DEATH(set.free_list(2u), "");
}

TEST_P(Policy, Capacity) {
    entt::sparse_set set{GetParam()};

    set.reserve(42);

    ASSERT_EQ(set.capacity(), 42u);
    ASSERT_TRUE(set.empty());

    set.reserve(0);

    ASSERT_EQ(set.capacity(), 42u);
    ASSERT_TRUE(set.empty());
}

TEST_P(Policy, Pagination) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{GetParam()};

    ASSERT_EQ(set.extent(), 0u);

    set.push(entt::entity{traits_type::page_size - 1u});

    ASSERT_EQ(set.extent(), traits_type::page_size);
    ASSERT_TRUE(set.contains(entt::entity{traits_type::page_size - 1u}));

    set.push(entt::entity{traits_type::page_size});

    ASSERT_EQ(set.extent(), 2 * traits_type::page_size);
    ASSERT_TRUE(set.contains(entt::entity{traits_type::page_size - 1u}));
    ASSERT_TRUE(set.contains(entt::entity{traits_type::page_size}));
    ASSERT_FALSE(set.contains(entt::entity{traits_type::page_size + 1u}));

    set.erase(entt::entity{traits_type::page_size - 1u});

    ASSERT_EQ(set.extent(), 2 * traits_type::page_size);
    ASSERT_FALSE(set.contains(entt::entity{traits_type::page_size - 1u}));
    ASSERT_TRUE(set.contains(entt::entity{traits_type::page_size}));

    set.shrink_to_fit();
    set.erase(entt::entity{traits_type::page_size});

    ASSERT_EQ(set.extent(), 2 * traits_type::page_size);
    ASSERT_FALSE(set.contains(entt::entity{traits_type::page_size - 1u}));
    ASSERT_FALSE(set.contains(entt::entity{traits_type::page_size}));

    set.shrink_to_fit();

    ASSERT_EQ(set.extent(), 2 * traits_type::page_size);
}

TEST(SwapAndPop, Contiguous) {
    entt::sparse_set set{entt::deletion_policy::swap_and_pop};

    const entt::entity entity{42};
    const entt::entity other{3};

    ASSERT_TRUE(set.contiguous());

    set.push(entity);
    set.push(other);

    ASSERT_TRUE(set.contiguous());

    set.erase(entity);

    ASSERT_TRUE(set.contiguous());

    set.clear();

    ASSERT_TRUE(set.contiguous());
}

TEST(InPlace, Contiguous) {
    entt::sparse_set set{entt::deletion_policy::in_place};

    const entt::entity entity{42};
    const entt::entity other{3};

    ASSERT_TRUE(set.contiguous());

    set.push(entity);
    set.push(other);

    ASSERT_TRUE(set.contiguous());

    set.erase(entity);

    ASSERT_FALSE(set.contiguous());

    set.compact();

    ASSERT_TRUE(set.contiguous());

    set.push(entity);
    set.erase(entity);

    ASSERT_FALSE(set.contiguous());

    set.clear();

    ASSERT_TRUE(set.contiguous());
}

TEST(SwapOnly, Contiguous) {
    entt::sparse_set set{entt::deletion_policy::swap_only};

    const entt::entity entity{42};
    const entt::entity other{3};

    ASSERT_TRUE(set.contiguous());

    set.push(entity);
    set.push(other);

    ASSERT_TRUE(set.contiguous());

    set.erase(entity);

    ASSERT_TRUE(set.contiguous());

    set.clear();

    ASSERT_TRUE(set.contiguous());
}

TEST(SwapAndPop, Data) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{entt::deletion_policy::swap_and_pop};

    const entt::entity entity{3};
    const entt::entity other{42};

    ASSERT_EQ(set.data(), nullptr);

    set.push(entity);
    set.push(other);
    set.erase(entity);

    ASSERT_FALSE(set.contains(entity));
    ASSERT_FALSE(set.contains(traits_type::next(entity)));

    ASSERT_EQ(set.size(), 1u);

    ASSERT_EQ(set.index(other), 0u);

    ASSERT_EQ(set.data()[0u], other);
}

TEST(InPlace, Data) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{entt::deletion_policy::in_place};

    const entt::entity entity{3};
    const entt::entity other{42};

    ASSERT_EQ(set.data(), nullptr);

    set.push(entity);
    set.push(other);
    set.erase(entity);

    ASSERT_FALSE(set.contains(entity));
    ASSERT_FALSE(set.contains(traits_type::next(entity)));

    ASSERT_EQ(set.size(), 2u);

    ASSERT_EQ(set.index(other), 1u);

    ASSERT_EQ(set.data()[0u], static_cast<entt::entity>(entt::tombstone));
    ASSERT_EQ(set.data()[1u], other);
}

TEST(SwapOnly, Data) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{entt::deletion_policy::swap_only};

    const entt::entity entity{3};
    const entt::entity other{42};

    ASSERT_EQ(set.data(), nullptr);

    set.push(entity);
    set.push(other);
    set.erase(entity);

    ASSERT_FALSE(set.contains(entity));
    ASSERT_TRUE(set.contains(traits_type::next(entity)));

    ASSERT_EQ(set.size(), 2u);

    ASSERT_EQ(set.index(other), 0u);
    ASSERT_EQ(set.index(traits_type::next(entity)), 1u);

    ASSERT_EQ(set.data()[0u], other);
    ASSERT_EQ(set.data()[1u], traits_type::next(entity));
}

TEST_P(Policy, Bind) {
    entt::sparse_set set{GetParam()};

    ASSERT_NO_FATAL_FAILURE(set.bind(entt::any{}));
}

TEST_P(Policy, Iterator) {
    using iterator = typename entt::sparse_set::iterator;

    testing::StaticAssertTypeEq<iterator::value_type, entt::entity>();
    testing::StaticAssertTypeEq<iterator::pointer, const entt::entity *>();
    testing::StaticAssertTypeEq<iterator::reference, const entt::entity &>();

    entt::sparse_set set{GetParam()};
    set.push(entt::entity{3});

    iterator end{set.begin()};
    iterator begin{};

    ASSERT_EQ(end.data(), set.data());
    ASSERT_EQ(begin.data(), nullptr);

    begin = set.end();
    std::swap(begin, end);

    ASSERT_EQ(end.data(), set.data());
    ASSERT_EQ(begin.data(), set.data());

    ASSERT_EQ(begin, set.cbegin());
    ASSERT_EQ(end, set.cend());
    ASSERT_NE(begin, end);

    ASSERT_EQ(begin.index(), 0);
    ASSERT_EQ(end.index(), -1);

    ASSERT_EQ(begin++, set.begin());
    ASSERT_EQ(begin--, set.end());

    ASSERT_EQ(begin + 1, set.end());
    ASSERT_EQ(end - 1, set.begin());

    ASSERT_EQ(++begin, set.end());
    ASSERT_EQ(--begin, set.begin());

    ASSERT_EQ(begin += 1, set.end());
    ASSERT_EQ(begin -= 1, set.begin());

    ASSERT_EQ(begin + (end - begin), set.end());
    ASSERT_EQ(begin - (begin - end), set.end());

    ASSERT_EQ(end - (end - begin), set.begin());
    ASSERT_EQ(end + (begin - end), set.begin());

    ASSERT_EQ(begin[0u], *set.begin());

    ASSERT_LT(begin, end);
    ASSERT_LE(begin, set.begin());

    ASSERT_GT(end, begin);
    ASSERT_GE(end, set.end());

    ASSERT_EQ(*begin, entt::entity{3});
    ASSERT_EQ(*begin.operator->(), entt::entity{3});

    ASSERT_EQ(begin.index(), 0);
    ASSERT_EQ(end.index(), -1);

    set.push(entt::entity{42});
    begin = set.begin();

    ASSERT_EQ(begin.index(), 1);
    ASSERT_EQ(end.index(), -1);

    ASSERT_EQ(begin[0u], entt::entity{42});
    ASSERT_EQ(begin[1u], entt::entity{3});
}

TEST_P(Policy, ReverseIterator) {
    using reverse_iterator = typename entt::sparse_set::reverse_iterator;

    testing::StaticAssertTypeEq<reverse_iterator::value_type, entt::entity>();
    testing::StaticAssertTypeEq<reverse_iterator::pointer, const entt::entity *>();
    testing::StaticAssertTypeEq<reverse_iterator::reference, const entt::entity &>();

    entt::sparse_set set{GetParam()};
    set.push(entt::entity{3});

    reverse_iterator end{set.rbegin()};
    reverse_iterator begin{};
    begin = set.rend();
    std::swap(begin, end);

    ASSERT_EQ(begin, set.crbegin());
    ASSERT_EQ(end, set.crend());
    ASSERT_NE(begin, end);

    ASSERT_EQ(begin.base().index(), -1);
    ASSERT_EQ(end.base().index(), 0);

    ASSERT_EQ(begin++, set.rbegin());
    ASSERT_EQ(begin--, set.rend());

    ASSERT_EQ(begin + 1, set.rend());
    ASSERT_EQ(end - 1, set.rbegin());

    ASSERT_EQ(++begin, set.rend());
    ASSERT_EQ(--begin, set.rbegin());

    ASSERT_EQ(begin += 1, set.rend());
    ASSERT_EQ(begin -= 1, set.rbegin());

    ASSERT_EQ(begin + (end - begin), set.rend());
    ASSERT_EQ(begin - (begin - end), set.rend());

    ASSERT_EQ(end - (end - begin), set.rbegin());
    ASSERT_EQ(end + (begin - end), set.rbegin());

    ASSERT_EQ(begin[0u], *set.rbegin());

    ASSERT_LT(begin, end);
    ASSERT_LE(begin, set.rbegin());

    ASSERT_GT(end, begin);
    ASSERT_GE(end, set.rend());

    ASSERT_EQ(*begin, entt::entity{3});
    ASSERT_EQ(*begin.operator->(), entt::entity{3});

    ASSERT_EQ(begin.base().index(), -1);
    ASSERT_EQ(end.base().index(), 0);

    set.push(entt::entity{42});
    end = set.rend();

    ASSERT_EQ(begin.base().index(), -1);
    ASSERT_EQ(end.base().index(), 1);

    ASSERT_EQ(begin[0u], entt::entity{3});
    ASSERT_EQ(begin[1u], entt::entity{42});
}

TEST(SwapAndPop, ScopedIterator) {
    entt::sparse_set set{entt::deletion_policy::swap_and_pop};

    const entt::entity entity{3};
    const entt::entity other{42};

    set.push(entity);
    set.push(other);
    set.erase(entity);

    ASSERT_EQ(set.begin(), set.begin(0));
    ASSERT_EQ(set.end(), set.end(0));
    ASSERT_NE(set.cbegin(0), set.cend(0));
}

TEST(InPlace, ScopedIterator) {
    entt::sparse_set set{entt::deletion_policy::in_place};

    const entt::entity entity{3};
    const entt::entity other{42};

    set.push(entity);
    set.push(other);
    set.erase(entity);

    ASSERT_EQ(set.begin(), set.begin(0));
    ASSERT_EQ(set.end(), set.end(0));
    ASSERT_NE(set.cbegin(0), set.cend(0));
}

TEST(SwapOnly, ScopedIterator) {
    entt::sparse_set set{entt::deletion_policy::swap_only};

    const entt::entity entity{3};
    const entt::entity other{42};

    set.push(entity);
    set.push(other);
    set.erase(entity);

    ASSERT_NE(set.begin(), set.begin(0));
    ASSERT_EQ(set.begin() + 1, set.begin(0));
    ASSERT_EQ(set.end(), set.end(0));
    ASSERT_NE(set.cbegin(0), set.cend(0));

    set.free_list(0);

    ASSERT_NE(set.begin(), set.begin(0));
    ASSERT_EQ(set.begin() + 2, set.begin(0));
    ASSERT_EQ(set.end(), set.end(0));
    ASSERT_EQ(set.cbegin(0), set.cend(0));

    set.free_list(2);

    ASSERT_EQ(set.begin(), set.begin(0));
    ASSERT_EQ(set.end(), set.end(0));
    ASSERT_NE(set.cbegin(0), set.cend(0));
}

TEST(SwapAndPop, ScopedReverseIterator) {
    entt::sparse_set set{entt::deletion_policy::swap_and_pop};

    const entt::entity entity{3};
    const entt::entity other{42};

    set.push(entity);
    set.push(other);
    set.erase(entity);

    ASSERT_EQ(set.rbegin(), set.rbegin(0));
    ASSERT_EQ(set.rend(), set.rend(0));
    ASSERT_NE(set.crbegin(0), set.crend(0));
}

TEST(InPlace, ScopedReverseIterator) {
    entt::sparse_set set{entt::deletion_policy::in_place};

    const entt::entity entity{3};
    const entt::entity other{42};

    set.push(entity);
    set.push(other);
    set.erase(entity);

    ASSERT_EQ(set.rbegin(), set.rbegin(0));
    ASSERT_EQ(set.rend(), set.rend(0));
    ASSERT_NE(set.crbegin(0), set.crend(0));
}

TEST(SwapOnly, ScopedReverseIterator) {
    entt::sparse_set set{entt::deletion_policy::swap_only};

    const entt::entity entity{3};
    const entt::entity other{42};

    set.push(entity);
    set.push(other);
    set.erase(entity);

    ASSERT_EQ(set.rbegin(), set.rbegin(0));
    ASSERT_NE(set.rend(), set.rend(0));
    ASSERT_EQ(set.rend() - 1, set.rend(0));
    ASSERT_NE(set.crbegin(0), set.crend(0));

    set.free_list(0);

    ASSERT_EQ(set.rbegin(), set.rbegin(0));
    ASSERT_NE(set.rend(), set.rend(0));
    ASSERT_EQ(set.rend() - 2, set.rend(0));
    ASSERT_EQ(set.crbegin(0), set.crend(0));

    set.free_list(2);

    ASSERT_EQ(set.rbegin(), set.rbegin(0));
    ASSERT_EQ(set.rend(), set.rend(0));
    ASSERT_NE(set.crbegin(0), set.crend(0));
}

TEST_P(Policy, Find) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{GetParam()};

    ASSERT_EQ(set.find(entt::tombstone), set.cend());
    ASSERT_EQ(set.find(entt::null), set.cend());

    const entt::entity entity{3};
    const entt::entity other{traits_type::construct(99, 1)};

    ASSERT_EQ(set.find(entity), set.cend());
    ASSERT_EQ(set.find(other), set.cend());

    set.push(entity);
    set.push(other);

    ASSERT_NE(set.find(entity), set.end());
    ASSERT_EQ(set.find(traits_type::next(entity)), set.end());
    ASSERT_EQ(*set.find(other), other);
}

TEST(SwapAndPop, FindErased) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{entt::deletion_policy::swap_and_pop};

    const entt::entity entity{3};

    set.push(entity);
    set.erase(entity);

    ASSERT_EQ(set.find(entity), set.cend());
    ASSERT_EQ(set.find(traits_type::next(entity)), set.cend());
}

TEST(InPlace, FindErased) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{entt::deletion_policy::in_place};

    const entt::entity entity{3};

    set.push(entity);
    set.erase(entity);

    ASSERT_EQ(set.find(entity), set.cend());
    ASSERT_EQ(set.find(traits_type::next(entity)), set.cend());
}

TEST(SwapOnly, FindErased) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{entt::deletion_policy::swap_only};

    const entt::entity entity{3};

    set.push(entity);
    set.erase(entity);

    ASSERT_EQ(set.find(entity), set.cend());
    ASSERT_NE(set.find(traits_type::next(entity)), set.cend());
}

TEST_P(Policy, Contains) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{GetParam()};

    const entt::entity entity{3};
    const entt::entity other{traits_type::construct(99, 1)};

    set.push(entity);
    set.push(other);

    ASSERT_FALSE(set.contains(entt::null));
    ASSERT_FALSE(set.contains(entt::tombstone));

    ASSERT_TRUE(set.contains(entity));
    ASSERT_TRUE(set.contains(other));

    ASSERT_FALSE(set.contains(entt::entity{1}));
    ASSERT_FALSE(set.contains(traits_type::construct(3, 1)));
    ASSERT_FALSE(set.contains(traits_type::construct(99, traits_type::to_version(entt::tombstone))));

    set.erase(entity);
    set.remove(other);

    ASSERT_FALSE(set.contains(entity));
    ASSERT_FALSE(set.contains(other));
}

TEST(SwapAndPop, ContainsErased) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{entt::deletion_policy::swap_and_pop};

    const entt::entity entity{3};

    set.push(entity);
    set.erase(entity);

    ASSERT_EQ(set.size(), 0u);
    ASSERT_FALSE(set.contains(entity));
    ASSERT_FALSE(set.contains(traits_type::next(entity)));
}

TEST(InPlace, ContainsErased) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{entt::deletion_policy::in_place};

    const entt::entity entity{3};

    set.push(entity);
    set.erase(entity);

    ASSERT_EQ(set.size(), 1u);
    ASSERT_FALSE(set.contains(entity));
    ASSERT_FALSE(set.contains(traits_type::next(entity)));
}

TEST(SwapOnly, ContainsErased) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{entt::deletion_policy::swap_only};

    const entt::entity entity{3};

    set.push(entity);
    set.erase(entity);

    ASSERT_EQ(set.size(), 1u);
    ASSERT_FALSE(set.contains(entity));
    ASSERT_TRUE(set.contains(traits_type::next(entity)));
}

TEST_P(Policy, Current) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{GetParam()};

    ASSERT_EQ(set.current(entt::tombstone), traits_type::to_version(entt::tombstone));
    ASSERT_EQ(set.current(entt::null), traits_type::to_version(entt::tombstone));

    const entt::entity entity{traits_type::construct(0, 0)};
    const entt::entity other{traits_type::construct(3, 3)};

    ASSERT_EQ(set.current(entity), traits_type::to_version(entt::tombstone));
    ASSERT_EQ(set.current(other), traits_type::to_version(entt::tombstone));

    set.push(entity);
    set.push(other);

    ASSERT_NE(set.current(entity), traits_type::to_version(entt::tombstone));
    ASSERT_NE(set.current(other), traits_type::to_version(entt::tombstone));

    ASSERT_EQ(set.current(traits_type::next(entity)), traits_type::to_version(entity));
    ASSERT_EQ(set.current(traits_type::next(other)), traits_type::to_version(other));
}

TEST(SwapAndPop, CurrentErased) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{entt::deletion_policy::swap_and_pop};

    const entt::entity entity{traits_type::construct(3, 3)};

    set.push(entity);
    set.erase(entity);

    ASSERT_EQ(set.size(), 0u);
    ASSERT_EQ(set.current(entity), traits_type::to_version(entt::tombstone));
}

TEST(InPlace, CurrentErased) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{entt::deletion_policy::in_place};

    const entt::entity entity{traits_type::construct(3, 3)};

    set.push(entity);
    set.erase(entity);

    ASSERT_EQ(set.size(), 1u);
    ASSERT_EQ(set.current(entity), traits_type::to_version(entt::tombstone));
}

TEST(SwapOnly, CurrentErased) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{entt::deletion_policy::swap_only};

    const entt::entity entity{traits_type::construct(3, 3)};

    set.push(entity);
    set.erase(entity);

    ASSERT_EQ(set.size(), 1u);
    ASSERT_EQ(set.current(entity), traits_type::to_version(traits_type::next(entity)));
}

TEST(SwapAndPop, Index) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{entt::deletion_policy::swap_and_pop};

    const entt::entity entity{42};
    const entt::entity other{3};

    set.push(entity);
    set.push(other);

    ASSERT_EQ(set.index(entity), 0u);
    ASSERT_EQ(set.index(other), 1u);

    set.erase(entity);

    ASSERT_EQ(set.size(), 1u);
    ASSERT_FALSE(set.contains(traits_type::next(entity)));
    ASSERT_EQ(set.index(other), 0u);
}

TEST(InPlace, Index) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{entt::deletion_policy::in_place};

    const entt::entity entity{42};
    const entt::entity other{3};

    set.push(entity);
    set.push(other);

    ASSERT_EQ(set.index(entity), 0u);
    ASSERT_EQ(set.index(other), 1u);

    set.erase(entity);

    ASSERT_EQ(set.size(), 2u);
    ASSERT_FALSE(set.contains(traits_type::next(entity)));
    ASSERT_EQ(set.index(other), 1u);
}

TEST(SwapOnly, Index) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{entt::deletion_policy::swap_only};

    const entt::entity entity{42};
    const entt::entity other{3};

    set.push(entity);
    set.push(other);

    ASSERT_EQ(set.index(entity), 0u);
    ASSERT_EQ(set.index(other), 1u);

    set.erase(entity);

    ASSERT_EQ(set.size(), 2u);
    ASSERT_TRUE(set.contains(traits_type::next(entity)));
    ASSERT_EQ(set.index(traits_type::next(entity)), 1u);
    ASSERT_EQ(set.index(other), 0u);
}

ENTT_DEBUG_TEST_P(PolicyDeathTest, Index) {
    using traits_type = entt::entt_traits<entt::entity>;

    entt::sparse_set set{GetParam()};

    ASSERT_DEATH(static_cast<void>(set.index(traits_type::construct(3, 0))), "");
    ASSERT_DEATH(static_cast<void>(set.index(entt::null)), "");
}

TEST_P(Policy, Indexing) {
    entt::sparse_set set{GetParam()};

    ASSERT_EQ(set.size(), 0u);

    ASSERT_EQ(set.at(0u), static_cast<entt::entity>(entt::null));
    ASSERT_EQ(set.at(99u), static_cast<entt::entity>(entt::null));

    const entt::entity entity{42};
    const entt::entity other{3};

    set.push(entity);
    set.push(other);

    ASSERT_EQ(set.size(), 2u);

    ASSERT_EQ(set.at(0u), entity);
    ASSERT_EQ(set.at(1u), other);

    ASSERT_EQ(set.at(0u), set[0u]);
    ASSERT_EQ(set.at(1u), set[1u]);

    ASSERT_EQ(set.at(0u), set.data()[0u]);
    ASSERT_EQ(set.at(1u), set.data()[1u]);

    ASSERT_EQ(set.at(2u), static_cast<entt::entity>(entt::null));
}

ENTT_DEBUG_TEST_P(PolicyDeathTest, Indexing) {
    entt::sparse_set set{GetParam()};

    ASSERT_DEATH([[maybe_unused]] auto value = set[0u], "");

    const entt::entity entity{42};

    set.push(entity);

    ASSERT_EQ(set[0u], entity);
    ASSERT_DEATH([[maybe_unused]] auto value = set[1u], "");
}

TEST_P(Policy, Value) {
    entt::sparse_set set{GetParam()};

    const entt::entity entity{3};

    set.push(entity);

    ASSERT_EQ(set.value(entity), nullptr);
    ASSERT_EQ(std::as_const(set).value(entity), nullptr);
}

ENTT_DEBUG_TEST_P(PolicyDeathTest, Value) {
    entt::sparse_set set{GetParam()};

    ASSERT_DEATH([[maybe_unused]] auto *value = set.value(entt::entity{3}), "");
}

TEST(SwapAndPop, Push) {
    entt::sparse_set set{entt::deletion_policy::swap_and_pop};

    const entt::entity entity[2u]{entt::entity{3}, entt::entity{42}};

    ASSERT_EQ(set.size(), 0u);
    ASSERT_EQ(*set.push(entity[0u]), entity[0u]);
    ASSERT_EQ(*set.push(entity[1u]), entity[1u]);
    ASSERT_EQ(set.size(), 2u);

    ASSERT_EQ(set.at(0u), entity[0u]);
    ASSERT_EQ(set.at(1u), entity[1u]);

    set.erase(std::begin(entity), std::end(entity));

    ASSERT_EQ(set.size(), 0u);
    ASSERT_EQ(*set.push(entity[0u]), entity[0u]);
    ASSERT_EQ(*set.push(entity[1u]), entity[1u]);
    ASSERT_EQ(set.size(), 2u);

    ASSERT_EQ(set.at(0u), entity[0u]);
    ASSERT_EQ(set.at(1u), entity[1u]);

    set.erase(std::begin(entity), std::end(entity));

    ASSERT_EQ(set.size(), 0u);
    ASSERT_EQ(*set.push(std::begin(entity), std::end(entity)), entity[0u]);
    ASSERT_EQ(set.size(), 2u);

    ASSERT_EQ(set.at(0u), entity[0u]);
    ASSERT_EQ(set.at(1u), entity[1u]);

    set.erase(std::begin(entity), std::end(entity));

    ASSERT_EQ(set.size(), 0u);
    ASSERT_EQ(set.push(std::begin(entity), std::begin(entity)), set.end());
    ASSERT_EQ(set.size(), 0u);
}

TEST(InPlace, Push) {
    entt::sparse_set set{entt::deletion_policy::in_place};

    const entt::entity entity[2u]{entt::entity{3}, entt::entity{42}};

    ASSERT_EQ(set.size(), 0u);
    ASSERT_EQ(*set.push(entity[0u]), entity[0u]);
    ASSERT_EQ(*set.push(entity[1u]), entity[1u]);
    ASSERT_EQ(set.size(), 2u);

    ASSERT_EQ(set.at(0u), entity[0u]);
    ASSERT_EQ(set.at(1u), entity[1u]);

    set.erase(std::begin(entity), std::end(entity));

    ASSERT_EQ(set.size(), 2u);
    ASSERT_EQ(*set.push(entity[0u]), entity[0u]);
    ASSERT_EQ(*set.push(entity[1u]), entity[1u]);
    ASSERT_EQ(set.size(), 2u);

    ASSERT_EQ(set.at(0u), entity[1u]);
    ASSERT_EQ(set.at(1u), entity[0u]);

    set.erase(std::begin(entity), std::end(entity));

    ASSERT_EQ(set.size(), 2u);
    ASSERT_EQ(*set.push(std::begin(entity), std::end(entity)), entity[0u]);
    ASSERT_EQ(set.size(), 4u);

    ASSERT_EQ(set.at(2u), entity[0u]);
    ASSERT_EQ(set.at(3u), entity[1u]);

    set.erase(std::begin(entity), std::end(entity));
    set.compact();

    ASSERT_EQ(set.size(), 0u);
    ASSERT_EQ(set.push(std::begin(entity), std::begin(entity)), set.end());
    ASSERT_EQ(set.size(), 0u);
}

TEST(SwapOnly, Push) {
    entt::sparse_set set{entt::deletion_policy::swap_only};

    const entt::entity entity[2u]{entt::entity{3}, entt::entity{42}};

    ASSERT_EQ(set.size(), 0u);
    ASSERT_EQ(set.free_list(), 0u);
    ASSERT_EQ(*set.push(entity[0u]), entity[0u]);
    ASSERT_EQ(*set.push(entity[1u]), entity[1u]);
    ASSERT_EQ(set.free_list(), 2u);
    ASSERT_EQ(set.size(), 2u);

    ASSERT_EQ(set.at(0u), entity[0u]);
    ASSERT_EQ(set.at(1u), entity[1u]);

    set.erase(std::begin(entity), std::end(entity));

    ASSERT_EQ(set.size(), 2u);
    ASSERT_EQ(set.free_list(), 0u);
    ASSERT_EQ(*set.push(entity[0u]), entity[0u]);
    ASSERT_EQ(*set.push(entity[1u]), entity[1u]);
    ASSERT_EQ(set.free_list(), 2u);
    ASSERT_EQ(set.size(), 2u);

    ASSERT_EQ(set.at(0u), entity[0u]);
    ASSERT_EQ(set.at(1u), entity[1u]);

    set.erase(std::begin(entity), std::end(entity));

    ASSERT_EQ(set.size(), 2u);
    ASSERT_EQ(set.free_list(), 0u);
    ASSERT_EQ(*set.push(std::begin(entity), std::end(entity)), entity[0u]);
    ASSERT_EQ(set.free_list(), 2u);
    ASSERT_EQ(set.size(), 2u);

    ASSERT_EQ(set.at(0u), entity[0u]);
    ASSERT_EQ(set.at(1u), entity[1u]);

    set.erase(std::begin(entity), std::end(entity));

    ASSERT_EQ(set.size(), 2u);
    ASSERT_EQ(set.free_list(), 0u);
    ASSERT_EQ(set.push(std::begin(entity), std::begin(entity)), set.end());
    ASSERT_EQ(set.free_list(), 0u);
    ASSERT_EQ(set.size(), 2u);
}

TEST_P(Policy, PushOutOfBounds) {
    using traits_type = entt::entt_traits<entt::entity>;

    entt::sparse_set set{GetParam()};

    const entt::entity entity[2u]{entt::entity{0}, entt::entity{traits_type::page_size}};

    ASSERT_EQ(*set.push(entity[0u]), entity[0u]);
    ASSERT_EQ(set.extent(), traits_type::page_size);
    ASSERT_EQ(set.index(entity[0u]), 0u);

    set.erase(entity[0u]);

    ASSERT_EQ(*set.push(entity[1u]), entity[1u]);
    ASSERT_EQ(set.extent(), 2u * traits_type::page_size);
    ASSERT_EQ(set.index(entity[1u]), 0u);
}

ENTT_DEBUG_TEST_P(PolicyDeathTest, Push) {
    entt::sparse_set set{GetParam()};

    const entt::entity entity[2u]{entt::entity{3}, entt::entity{42}};

    set.push(std::begin(entity), std::end(entity));

    ASSERT_DEATH(set.push(entity[0u]), "");
    ASSERT_DEATH(set.push(std::begin(entity), std::end(entity)), "");

    set.erase(entity[1u]);

    ASSERT_DEATH(set.push(entity[0u]), "");
    ASSERT_DEATH(set.push(std::begin(entity), std::end(entity)), "");
}

TEST_P(Policy, Bump) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{GetParam()};

    const entt::entity entity[3u]{entt::entity{3}, entt::entity{42}, traits_type::construct(9, 3)};

    set.push(std::begin(entity), std::end(entity));

    ASSERT_EQ(set.current(entity[0u]), 0u);
    ASSERT_EQ(set.current(entity[1u]), 0u);
    ASSERT_EQ(set.current(entity[2u]), 3u);

    ASSERT_EQ(set.bump(entity[0u]), 0u);
    ASSERT_EQ(set.bump(traits_type::construct(traits_type::to_entity(entity[1u]), 1)), 1u);
    ASSERT_EQ(set.bump(traits_type::construct(traits_type::to_entity(entity[2u]), 0)), 0u);

    ASSERT_EQ(set.current(entity[0u]), 0u);
    ASSERT_EQ(set.current(entity[1u]), 1u);
    ASSERT_EQ(set.current(entity[2u]), 0u);
}

ENTT_DEBUG_TEST_P(PolicyDeathTest, Bump) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{GetParam()};

    set.push(entt::entity{3});

    ASSERT_DEATH(set.bump(entt::null), "");
    ASSERT_DEATH(set.bump(entt::tombstone), "");
    ASSERT_DEATH(set.bump(entt::entity{42}), "");
    ASSERT_DEATH(set.bump(traits_type::construct(traits_type::to_entity(entt::entity{3}), traits_type::to_version(entt::tombstone))), "");
}

TEST(SwapAndPop, Erase) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{entt::deletion_policy::swap_and_pop};

    entt::entity entity[3u]{entt::entity{3}, entt::entity{42}, traits_type::construct(9, 3)};

    ASSERT_EQ(set.size(), 0u);
    ASSERT_EQ(set.free_list(), traits_type::entity_mask);

    set.push(std::begin(entity), std::end(entity));
    set.erase(set.begin(), set.end());

    ASSERT_EQ(set.size(), 0u);
    ASSERT_EQ(set.free_list(), traits_type::entity_mask);

    set.push(std::begin(entity), std::end(entity));
    set.erase(entity, entity + 2u);

    ASSERT_EQ(set.size(), 1u);
    ASSERT_EQ(set.free_list(), traits_type::entity_mask);
    ASSERT_TRUE(set.contains(entity[2u]));

    set.erase(entity[2u]);

    ASSERT_EQ(set.size(), 0u);
    ASSERT_EQ(set.free_list(), traits_type::entity_mask);
    ASSERT_FALSE(set.contains(entity[2u]));
}

TEST(InPlace, Erase) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{entt::deletion_policy::in_place};

    entt::entity entity[3u]{entt::entity{3}, entt::entity{42}, traits_type::construct(9, 3)};

    ASSERT_EQ(set.size(), 0u);
    ASSERT_EQ(set.free_list(), traits_type::entity_mask);

    set.push(std::begin(entity), std::end(entity));
    set.erase(set.begin(), set.end());

    ASSERT_EQ(set.size(), 3u);
    ASSERT_EQ(set.free_list(), 0u);

    ASSERT_EQ(set.current(entity[0u]), traits_type::to_version(entt::tombstone));
    ASSERT_EQ(set.current(entity[1u]), traits_type::to_version(entt::tombstone));
    ASSERT_EQ(set.current(entity[2u]), traits_type::to_version(entt::tombstone));

    set.push(entity[0u]);
    set.push(std::begin(entity) + 1, std::end(entity));
    set.erase(entity, entity + 2u);

    ASSERT_EQ(set.size(), 5u);
    ASSERT_EQ(set.free_list(), 3u);

    ASSERT_EQ(set.current(entity[0u]), traits_type::to_version(entt::tombstone));
    ASSERT_EQ(set.current(entity[1u]), traits_type::to_version(entt::tombstone));
    ASSERT_TRUE(set.contains(entity[2u]));

    set.erase(entity[2u]);

    ASSERT_EQ(set.size(), 5u);
    ASSERT_EQ(set.free_list(), 4u);
    ASSERT_FALSE(set.contains(entity[2u]));
}

TEST(SwapOnly, Erase) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{entt::deletion_policy::swap_only};

    entt::entity entity[3u]{entt::entity{3}, entt::entity{42}, traits_type::construct(9, 3)};

    ASSERT_EQ(set.size(), 0u);
    ASSERT_EQ(set.free_list(), 0u);

    set.push(std::begin(entity), std::end(entity));
    set.erase(set.begin(), set.end());

    ASSERT_EQ(set.size(), 3u);
    ASSERT_EQ(set.free_list(), 0u);

    ASSERT_TRUE(set.contains(traits_type::next(entity[0u])));
    ASSERT_TRUE(set.contains(traits_type::next(entity[1u])));
    ASSERT_TRUE(set.contains(traits_type::next(entity[2u])));

    set.push(std::begin(entity), std::end(entity));
    set.erase(entity, entity + 2u);

    ASSERT_EQ(set.size(), 3u);
    ASSERT_EQ(set.free_list(), 1u);

    ASSERT_TRUE(set.contains(traits_type::next(entity[0u])));
    ASSERT_TRUE(set.contains(traits_type::next(entity[1u])));
    ASSERT_TRUE(set.contains(entity[2u]));

    ASSERT_LT(set.index(entity[2u]), set.free_list());

    set.erase(entity[2u]);

    ASSERT_EQ(set.size(), 3u);
    ASSERT_EQ(set.free_list(), 0u);
    ASSERT_TRUE(set.contains(traits_type::next(entity[2u])));
}

ENTT_DEBUG_TEST_P(PolicyDeathTest, Erase) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{GetParam()};

    entt::entity entity[2u]{entt::entity{42}, traits_type::construct(9, 3)};

    ASSERT_DEATH(set.erase(std::begin(entity), std::end(entity)), "");
    ASSERT_DEATH(set.erase(entity, entity + 2u), "");
    ASSERT_DEATH(set.erase(entt::null), "");
}

TEST_P(Policy, CrossErase) {
    entt::sparse_set set{GetParam()};
    entt::sparse_set other{GetParam()};

    entt::entity entity[2u]{entt::entity{3}, entt::entity{42}};

    set.push(std::begin(entity), std::end(entity));
    other.push(entity[1u]);
    set.erase(other.begin(), other.end());

    ASSERT_TRUE(set.contains(entity[0u]));
    ASSERT_FALSE(set.contains(entity[1u]));
    ASSERT_EQ(set.data()[0u], entity[0u]);
}

TEST(SwapAndPop, Remove) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{entt::deletion_policy::swap_and_pop};

    entt::entity entity[3u]{entt::entity{3}, entt::entity{42}, traits_type::construct(9, 3)};

    ASSERT_EQ(set.size(), 0u);
    ASSERT_EQ(set.free_list(), traits_type::entity_mask);

    ASSERT_EQ(set.remove(std::begin(entity), std::end(entity)), 0u);
    ASSERT_FALSE(set.remove(entity[1u]));

    ASSERT_EQ(set.size(), 0u);
    ASSERT_EQ(set.free_list(), traits_type::entity_mask);

    set.push(std::begin(entity), std::end(entity));

    ASSERT_EQ(set.remove(set.begin(), set.end()), 3u);

    ASSERT_EQ(set.size(), 0u);
    ASSERT_EQ(set.free_list(), traits_type::entity_mask);

    set.push(std::begin(entity), std::end(entity));

    ASSERT_EQ(set.remove(entity, entity + 2u), 2u);

    ASSERT_EQ(set.size(), 1u);
    ASSERT_EQ(set.free_list(), traits_type::entity_mask);
    ASSERT_TRUE(set.contains(entity[2u]));

    ASSERT_TRUE(set.remove(entity[2u]));
    ASSERT_FALSE(set.remove(entity[2u]));

    ASSERT_EQ(set.size(), 0u);
    ASSERT_EQ(set.free_list(), traits_type::entity_mask);
    ASSERT_FALSE(set.contains(entity[2u]));
}

TEST(InPlace, Remove) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{entt::deletion_policy::in_place};

    entt::entity entity[3u]{entt::entity{3}, entt::entity{42}, traits_type::construct(9, 3)};

    ASSERT_EQ(set.size(), 0u);
    ASSERT_EQ(set.free_list(), traits_type::entity_mask);

    ASSERT_EQ(set.remove(std::begin(entity), std::end(entity)), 0u);
    ASSERT_FALSE(set.remove(entity[1u]));

    ASSERT_EQ(set.size(), 0u);
    ASSERT_EQ(set.free_list(), traits_type::entity_mask);

    set.push(std::begin(entity), std::end(entity));

    ASSERT_EQ(set.remove(set.begin(), set.end()), 3u);

    ASSERT_EQ(set.size(), 3u);
    ASSERT_EQ(set.free_list(), 0u);

    ASSERT_EQ(set.current(entity[0u]), traits_type::to_version(entt::tombstone));
    ASSERT_EQ(set.current(entity[1u]), traits_type::to_version(entt::tombstone));
    ASSERT_EQ(set.current(entity[2u]), traits_type::to_version(entt::tombstone));

    set.push(entity[0u]);
    set.push(std::begin(entity) + 1, std::end(entity));

    ASSERT_EQ(set.remove(entity, entity + 2u), 2u);

    ASSERT_EQ(set.size(), 5u);
    ASSERT_EQ(set.free_list(), 3u);

    ASSERT_EQ(set.current(entity[0u]), traits_type::to_version(entt::tombstone));
    ASSERT_EQ(set.current(entity[1u]), traits_type::to_version(entt::tombstone));
    ASSERT_TRUE(set.contains(entity[2u]));

    ASSERT_TRUE(set.remove(entity[2u]));
    ASSERT_FALSE(set.remove(entity[2u]));

    ASSERT_EQ(set.size(), 5u);
    ASSERT_EQ(set.free_list(), 4u);
    ASSERT_FALSE(set.contains(entity[2u]));
}

TEST(SwapOnly, Remove) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{entt::deletion_policy::swap_only};

    entt::entity entity[3u]{entt::entity{3}, entt::entity{42}, traits_type::construct(9, 3)};

    ASSERT_EQ(set.size(), 0u);
    ASSERT_EQ(set.free_list(), 0u);

    ASSERT_EQ(set.remove(std::begin(entity), std::end(entity)), 0u);
    ASSERT_FALSE(set.remove(entity[1u]));

    ASSERT_EQ(set.size(), 0u);
    ASSERT_EQ(set.free_list(), 0u);

    set.push(std::begin(entity), std::end(entity));

    ASSERT_EQ(set.remove(set.begin(), set.end()), 3u);

    ASSERT_EQ(set.size(), 3u);
    ASSERT_EQ(set.free_list(), 0u);

    ASSERT_TRUE(set.contains(traits_type::next(entity[0u])));
    ASSERT_TRUE(set.contains(traits_type::next(entity[1u])));
    ASSERT_TRUE(set.contains(traits_type::next(entity[2u])));

    set.push(std::begin(entity), std::end(entity));

    ASSERT_EQ(set.remove(entity, entity + 2u), 2u);

    ASSERT_EQ(set.size(), 3u);
    ASSERT_EQ(set.free_list(), 1u);

    ASSERT_TRUE(set.contains(traits_type::next(entity[0u])));
    ASSERT_TRUE(set.contains(traits_type::next(entity[1u])));
    ASSERT_TRUE(set.contains(entity[2u]));

    ASSERT_LT(set.index(entity[2u]), set.free_list());

    ASSERT_TRUE(set.remove(entity[2u]));
    ASSERT_FALSE(set.remove(entity[2u]));

    ASSERT_EQ(set.size(), 3u);
    ASSERT_EQ(set.free_list(), 0u);
    ASSERT_TRUE(set.contains(traits_type::next(entity[2u])));

    ASSERT_TRUE(set.remove(traits_type::next(entity[2u])));

    ASSERT_TRUE(set.contains(traits_type::next(traits_type::next(entity[2u]))));
}

TEST_P(Policy, CrossRemove) {
    entt::sparse_set set{GetParam()};
    entt::sparse_set other{GetParam()};

    entt::entity entity[2u]{entt::entity{3}, entt::entity{42}};

    set.push(std::begin(entity), std::end(entity));
    other.push(entity[1u]);
    set.remove(other.begin(), other.end());

    ASSERT_TRUE(set.contains(entity[0u]));
    ASSERT_FALSE(set.contains(entity[1u]));
    ASSERT_EQ(set.data()[0u], entity[0u]);
}

TEST(SwapAndPop, Compact) {
    entt::sparse_set set{entt::deletion_policy::swap_and_pop};

    const entt::entity entity{3};
    const entt::entity other{42};

    set.push(entity);
    set.push(other);

    ASSERT_EQ(set.size(), 2u);
    ASSERT_EQ(set.index(entity), 0u);
    ASSERT_EQ(set.index(other), 1u);

    set.compact();

    ASSERT_EQ(set.size(), 2u);
    ASSERT_EQ(set.index(entity), 0u);
    ASSERT_EQ(set.index(other), 1u);

    set.erase(entity);

    ASSERT_EQ(set.size(), 1u);
    ASSERT_EQ(set.index(other), 0u);

    set.compact();

    ASSERT_EQ(set.size(), 1u);
    ASSERT_EQ(set.index(other), 0u);
}

TEST(InPlace, Compact) {
    entt::sparse_set set{entt::deletion_policy::in_place};

    const entt::entity entity{3};
    const entt::entity other{42};

    set.push(entity);
    set.push(other);

    ASSERT_EQ(set.size(), 2u);
    ASSERT_EQ(set.index(entity), 0u);
    ASSERT_EQ(set.index(other), 1u);

    set.compact();

    ASSERT_EQ(set.size(), 2u);
    ASSERT_EQ(set.index(entity), 0u);
    ASSERT_EQ(set.index(other), 1u);

    set.erase(other);

    ASSERT_EQ(set.size(), 2u);
    ASSERT_EQ(set.index(entity), 0u);

    set.compact();

    ASSERT_EQ(set.size(), 1u);
    ASSERT_EQ(set.index(entity), 0u);

    set.push(other);
    set.erase(entity);

    ASSERT_EQ(set.size(), 2u);
    ASSERT_EQ(set.index(other), 1u);

    set.compact();

    ASSERT_EQ(set.size(), 1u);
    ASSERT_EQ(set.index(other), 0u);

    set.compact();

    ASSERT_EQ(set.size(), 1u);
    ASSERT_EQ(set.index(other), 0u);
}

TEST(SwapOnly, Compact) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{entt::deletion_policy::swap_only};

    const entt::entity entity{3};
    const entt::entity other{42};

    set.push(entity);
    set.push(other);

    ASSERT_EQ(set.size(), 2u);
    ASSERT_EQ(set.index(entity), 0u);
    ASSERT_EQ(set.index(other), 1u);

    set.compact();

    ASSERT_EQ(set.size(), 2u);
    ASSERT_EQ(set.index(entity), 0u);
    ASSERT_EQ(set.index(other), 1u);

    set.erase(entity);

    ASSERT_EQ(set.size(), 2u);
    ASSERT_EQ(set.index(other), 0u);
    ASSERT_EQ(set.index(traits_type::next(entity)), 1u);

    set.compact();

    ASSERT_EQ(set.size(), 2u);
    ASSERT_EQ(set.index(other), 0u);
    ASSERT_EQ(set.index(traits_type::next(entity)), 1u);
}

TEST_P(Policy, SwapElements) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{GetParam()};

    const auto entity = traits_type::construct(3, 5);
    const auto other = traits_type::construct(42, 99);

    set.push(entity);
    set.push(other);

    ASSERT_EQ(set.index(entity), 0u);
    ASSERT_EQ(set.index(other), 1u);

    set.swap_elements(entity, other);

    ASSERT_EQ(set.index(entity), 1u);
    ASSERT_EQ(set.index(other), 0u);
}

ENTT_DEBUG_TEST_P(PolicyDeathTest, SwapElements) {
    entt::sparse_set set{GetParam()};

    ASSERT_TRUE(set.empty());
    ASSERT_DEATH(set.swap_elements(entt::entity{0}, entt::entity{1}), "");
}

ENTT_DEBUG_TEST(SwapOnlyDeathTest, SwapElements) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set set{entt::deletion_policy::swap_only};

    const auto entity = traits_type::construct(3, 5);
    const auto other = traits_type::construct(42, 99);

    set.push(entity);
    set.push(other);
    set.erase(entity);

    ASSERT_EQ(set.index(traits_type::next(entity)), 1u);
    ASSERT_EQ(set.index(other), 0u);

    ASSERT_DEATH(set.swap_elements(entity, other), "");
}

TEST_P(Policy, Clear) {
    entt::sparse_set set{GetParam()};

    entt::entity entity[3u]{entt::entity{3}, entt::entity{42}, entt::entity{9}};

    set.push(std::begin(entity), std::end(entity));
    set.erase(entity[1u]);
    set.clear();

    ASSERT_EQ(set.size(), 0u);
}

TEST_P(Policy, SortOrdered) {
    entt::sparse_set set{GetParam()};

    entt::entity entity[5u]{entt::entity{42}, entt::entity{12}, entt::entity{9}, entt::entity{7}, entt::entity{3}};

    set.push(std::begin(entity), std::end(entity));
    set.sort(std::less{});

    ASSERT_TRUE(std::equal(std::rbegin(entity), std::rend(entity), set.begin(), set.end()));
}

TEST_P(Policy, SortReverse) {
    entt::sparse_set set{GetParam()};

    entt::entity entity[5u]{entt::entity{3}, entt::entity{7}, entt::entity{9}, entt::entity{12}, entt::entity{42}};

    set.push(std::begin(entity), std::end(entity));
    set.sort(std::less{});

    ASSERT_TRUE(std::equal(std::begin(entity), std::end(entity), set.begin(), set.end()));
}

TEST_P(Policy, SortUnordered) {
    entt::sparse_set set{GetParam()};

    entt::entity entity[5u]{entt::entity{9}, entt::entity{7}, entt::entity{3}, entt::entity{12}, entt::entity{42}};

    set.push(std::begin(entity), std::end(entity));
    set.sort(std::less{});

    auto begin = set.begin();
    const auto end = set.end();

    ASSERT_EQ(*(begin++), entity[2u]);
    ASSERT_EQ(*(begin++), entity[1u]);
    ASSERT_EQ(*(begin++), entity[0u]);
    ASSERT_EQ(*(begin++), entity[3u]);
    ASSERT_EQ(*(begin++), entity[4u]);

    ASSERT_EQ(begin, end);
}

ENTT_DEBUG_TEST(InPlaceDeathTest, Sort) {
    entt::sparse_set set{entt::deletion_policy::in_place};

    entt::entity entity{42};
    entt::entity other{3};

    set.push(entity);
    set.push(other);
    set.erase(entity);

    ASSERT_DEATH(set.sort(std::less{});, "");
}

ENTT_DEBUG_TEST(SwapOnlyDeathTest, Sort) {
    entt::sparse_set set{entt::deletion_policy::swap_only};

    entt::entity entity{42};
    entt::entity other{3};

    set.push(entity);
    set.push(other);
    set.erase(entity);

    ASSERT_DEATH(set.sort(std::less{});, "");
}

TEST_P(Policy, SortN) {
    entt::sparse_set set{GetParam()};

    entt::entity entity[5u]{entt::entity{7}, entt::entity{9}, entt::entity{3}, entt::entity{12}, entt::entity{42}};

    set.push(std::begin(entity), std::end(entity));
    set.sort_n(0u, std::less{});

    ASSERT_TRUE(std::equal(std::rbegin(entity), std::rend(entity), set.begin(), set.end()));

    set.sort_n(2u, std::less{});

    ASSERT_EQ(set.data()[0u], entity[1u]);
    ASSERT_EQ(set.data()[1u], entity[0u]);

    set.sort_n(5u, std::less{});

    auto begin = set.begin();
    auto end = set.end();

    ASSERT_EQ(*(begin++), entity[2u]);
    ASSERT_EQ(*(begin++), entity[0u]);
    ASSERT_EQ(*(begin++), entity[1u]);
    ASSERT_EQ(*(begin++), entity[3u]);
    ASSERT_EQ(*(begin++), entity[4u]);

    ASSERT_EQ(begin, end);
}

ENTT_DEBUG_TEST(SwapAndPopDeathTest, SortN) {
    entt::sparse_set set{entt::deletion_policy::swap_and_pop};

    entt::entity entity{42};
    entt::entity other{3};

    ASSERT_DEATH(set.sort_n(1u, std::less{});, "");

    set.push(entity);
    set.push(other);
    set.erase(entity);

    ASSERT_EQ(set.size(), 1u);
    ASSERT_NO_FATAL_FAILURE(set.sort_n(1u, std::less{}));
}

ENTT_DEBUG_TEST(InPlaceDeathTest, SortN) {
    entt::sparse_set set{entt::deletion_policy::in_place};

    entt::entity entity{42};
    entt::entity other{3};

    ASSERT_DEATH(set.sort_n(1u, std::less{});, "");

    set.push(entity);
    set.push(other);
    set.erase(entity);

    ASSERT_EQ(set.size(), 2u);
    ASSERT_DEATH(set.sort_n(1u, std::less{});, "");
    ASSERT_DEATH(set.sort_n(2u, std::less{});, "");
}

ENTT_DEBUG_TEST(SwapOnlyDeathTest, SortN) {
    entt::sparse_set set{entt::deletion_policy::swap_only};

    entt::entity entity{42};
    entt::entity other{3};

    ASSERT_DEATH(set.sort_n(1u, std::less{});, "");

    set.push(entity);
    set.push(other);
    set.erase(entity);

    ASSERT_EQ(set.size(), 2u);
    ASSERT_NO_FATAL_FAILURE(set.sort_n(1u, std::less{}));
    ASSERT_DEATH(set.sort_n(2u, std::less{});, "");
}

TEST_P(Policy, SortAsDisjoint) {
    entt::sparse_set lhs{GetParam()};
    entt::sparse_set rhs{GetParam()};

    entt::entity lhs_entity[3u]{entt::entity{3}, entt::entity{12}, entt::entity{42}};

    lhs.push(std::begin(lhs_entity), std::end(lhs_entity));

    ASSERT_TRUE(std::equal(std::rbegin(lhs_entity), std::rend(lhs_entity), lhs.begin(), lhs.end()));

    lhs.sort_as(rhs);

    ASSERT_TRUE(std::equal(std::rbegin(lhs_entity), std::rend(lhs_entity), lhs.begin(), lhs.end()));
}

TEST_P(Policy, SortAsOverlap) {
    entt::sparse_set lhs{GetParam()};
    entt::sparse_set rhs{GetParam()};

    entt::entity lhs_entity[3u]{entt::entity{3}, entt::entity{12}, entt::entity{42}};
    entt::entity rhs_entity[1u]{entt::entity{12}};

    lhs.push(std::begin(lhs_entity), std::end(lhs_entity));
    rhs.push(std::begin(rhs_entity), std::end(rhs_entity));

    ASSERT_TRUE(std::equal(std::rbegin(lhs_entity), std::rend(lhs_entity), lhs.begin(), lhs.end()));
    ASSERT_TRUE(std::equal(std::rbegin(rhs_entity), std::rend(rhs_entity), rhs.begin(), rhs.end()));

    lhs.sort_as(rhs);

    auto begin = lhs.begin();
    auto end = lhs.end();

    ASSERT_EQ(*(begin++), lhs_entity[1u]);
    ASSERT_EQ(*(begin++), lhs_entity[2u]);
    ASSERT_EQ(*(begin++), lhs_entity[0u]);
    ASSERT_EQ(begin, end);
}

TEST_P(Policy, SortAsOrdered) {
    entt::sparse_set lhs{GetParam()};
    entt::sparse_set rhs{GetParam()};

    entt::entity lhs_entity[5u]{entt::entity{1}, entt::entity{2}, entt::entity{3}, entt::entity{4}, entt::entity{5}};
    entt::entity rhs_entity[6u]{entt::entity{6}, entt::entity{1}, entt::entity{2}, entt::entity{3}, entt::entity{4}, entt::entity{5}};

    lhs.push(std::begin(lhs_entity), std::end(lhs_entity));
    rhs.push(std::begin(rhs_entity), std::end(rhs_entity));

    ASSERT_TRUE(std::equal(std::rbegin(lhs_entity), std::rend(lhs_entity), lhs.begin(), lhs.end()));
    ASSERT_TRUE(std::equal(std::rbegin(rhs_entity), std::rend(rhs_entity), rhs.begin(), rhs.end()));

    rhs.sort_as(lhs);

    ASSERT_TRUE(std::equal(std::rbegin(rhs_entity), std::rend(rhs_entity), rhs.begin(), rhs.end()));
}

TEST_P(Policy, SortAsReverse) {
    entt::sparse_set lhs{GetParam()};
    entt::sparse_set rhs{GetParam()};

    entt::entity lhs_entity[5u]{entt::entity{1}, entt::entity{2}, entt::entity{3}, entt::entity{4}, entt::entity{5}};
    entt::entity rhs_entity[6u]{entt::entity{5}, entt::entity{4}, entt::entity{3}, entt::entity{2}, entt::entity{1}, entt::entity{6}};

    lhs.push(std::begin(lhs_entity), std::end(lhs_entity));
    rhs.push(std::begin(rhs_entity), std::end(rhs_entity));

    ASSERT_TRUE(std::equal(std::rbegin(lhs_entity), std::rend(lhs_entity), lhs.begin(), lhs.end()));
    ASSERT_TRUE(std::equal(std::rbegin(rhs_entity), std::rend(rhs_entity), rhs.begin(), rhs.end()));

    rhs.sort_as(lhs);

    auto begin = rhs.begin();
    auto end = rhs.end();

    ASSERT_EQ(*(begin++), rhs_entity[0u]);
    ASSERT_EQ(*(begin++), rhs_entity[1u]);
    ASSERT_EQ(*(begin++), rhs_entity[2u]);
    ASSERT_EQ(*(begin++), rhs_entity[3u]);
    ASSERT_EQ(*(begin++), rhs_entity[4u]);
    ASSERT_EQ(*(begin++), rhs_entity[5u]);
    ASSERT_EQ(begin, end);
}

TEST_P(Policy, SortAsUnordered) {
    entt::sparse_set lhs{GetParam()};
    entt::sparse_set rhs{GetParam()};

    entt::entity lhs_entity[5u]{entt::entity{1}, entt::entity{2}, entt::entity{3}, entt::entity{4}, entt::entity{5}};
    entt::entity rhs_entity[6u]{entt::entity{3}, entt::entity{2}, entt::entity{6}, entt::entity{1}, entt::entity{4}, entt::entity{5}};

    lhs.push(std::begin(lhs_entity), std::end(lhs_entity));
    rhs.push(std::begin(rhs_entity), std::end(rhs_entity));

    ASSERT_TRUE(std::equal(std::rbegin(lhs_entity), std::rend(lhs_entity), lhs.begin(), lhs.end()));
    ASSERT_TRUE(std::equal(std::rbegin(rhs_entity), std::rend(rhs_entity), rhs.begin(), rhs.end()));

    rhs.sort_as(lhs);

    auto begin = rhs.begin();
    auto end = rhs.end();

    ASSERT_EQ(*(begin++), rhs_entity[5u]);
    ASSERT_EQ(*(begin++), rhs_entity[4u]);
    ASSERT_EQ(*(begin++), rhs_entity[0u]);
    ASSERT_EQ(*(begin++), rhs_entity[1u]);
    ASSERT_EQ(*(begin++), rhs_entity[3u]);
    ASSERT_EQ(*(begin++), rhs_entity[2u]);
    ASSERT_EQ(begin, end);
}

TEST_P(Policy, SortAsInvalid) {
    using traits_type = entt::entt_traits<entt::entity>;
    entt::sparse_set lhs{GetParam()};
    entt::sparse_set rhs{GetParam()};

    entt::entity lhs_entity[3u]{entt::entity{1}, entt::entity{2}, traits_type::construct(3, 1)};
    entt::entity rhs_entity[3u]{entt::entity{2}, entt::entity{1}, traits_type::construct(3, 2)};

    lhs.push(std::begin(lhs_entity), std::end(lhs_entity));
    rhs.push(std::begin(rhs_entity), std::end(rhs_entity));

    ASSERT_TRUE(std::equal(std::rbegin(lhs_entity), std::rend(lhs_entity), lhs.begin(), lhs.end()));
    ASSERT_TRUE(std::equal(std::rbegin(rhs_entity), std::rend(rhs_entity), rhs.begin(), rhs.end()));

    rhs.sort_as(lhs);

    auto begin = rhs.begin();
    auto end = rhs.end();

    ASSERT_EQ(*(begin++), rhs_entity[0u]);
    ASSERT_EQ(*(begin++), rhs_entity[1u]);
    ASSERT_EQ(*(begin++), rhs_entity[2u]);
    ASSERT_EQ(rhs.current(rhs_entity[0u]), 0u);
    ASSERT_EQ(rhs.current(rhs_entity[1u]), 0u);
    ASSERT_EQ(rhs.current(rhs_entity[2u]), 2u);
    ASSERT_EQ(begin, end);
}

ENTT_DEBUG_TEST(InPlaceDeathTest, SortAs) {
    entt::sparse_set lhs{entt::deletion_policy::in_place};
    entt::sparse_set rhs{entt::deletion_policy::in_place};

    entt::entity entity{42};

    lhs.push(entity);
    lhs.erase(entity);

    ASSERT_DEATH(lhs.sort_as(rhs);, "");
}

ENTT_DEBUG_TEST(SwapOnlyDeathTest, SortAs) {
    entt::sparse_set lhs{entt::deletion_policy::swap_only};
    entt::sparse_set rhs{entt::deletion_policy::swap_only};

    entt::entity entity[2u]{entt::entity{3}, entt::entity{42}};

    lhs.push(std::begin(entity), std::end(entity));
    rhs.push(std::rbegin(entity), std::rend(entity));
    lhs.erase(entity[1u]);

    ASSERT_DEATH(lhs.sort_as(rhs);, "");
}

TEST_P(Policy, CanModifyDuringIteration) {
    entt::sparse_set set{GetParam()};
    set.push(entt::entity{0});

    ASSERT_EQ(set.capacity(), 1u);

    const auto it = set.begin();
    set.reserve(2u);

    ASSERT_EQ(set.capacity(), 2u);

    // this should crash with asan enabled if we break the constraint
    [[maybe_unused]] const auto entity = *it;
}

TEST_P(Policy, CustomAllocator) {
    test::throwing_allocator<entt::entity> allocator{};
    entt::basic_sparse_set<entt::entity, test::throwing_allocator<entt::entity>> set{GetParam(), allocator};

    ASSERT_EQ(set.get_allocator(), allocator);

    set.reserve(1u);

    ASSERT_EQ(set.capacity(), 1u);

    set.push(entt::entity{0});
    set.push(entt::entity{1});

    entt::basic_sparse_set<entt::entity, test::throwing_allocator<entt::entity>> other{std::move(set), allocator};

    ASSERT_TRUE(set.empty());
    ASSERT_FALSE(other.empty());
    ASSERT_EQ(set.capacity(), 0u);
    ASSERT_EQ(other.capacity(), 2u);
    ASSERT_EQ(other.size(), 2u);

    set = std::move(other);

    ASSERT_FALSE(set.empty());
    ASSERT_TRUE(other.empty());
    ASSERT_EQ(other.capacity(), 0u);
    ASSERT_EQ(set.capacity(), 2u);
    ASSERT_EQ(set.size(), 2u);

    set.swap(other);
    set = std::move(other);

    ASSERT_FALSE(set.empty());
    ASSERT_TRUE(other.empty());
    ASSERT_EQ(other.capacity(), 0u);
    ASSERT_EQ(set.capacity(), 2u);
    ASSERT_EQ(set.size(), 2u);

    set.clear();

    ASSERT_EQ(set.capacity(), 2u);
    ASSERT_EQ(set.size(), 0u);

    set.shrink_to_fit();

    ASSERT_EQ(set.capacity(), 0u);
}

TEST_P(Policy, ThrowingAllocator) {
    using traits_type = entt::entt_traits<entt::entity>;

    entt::basic_sparse_set<entt::entity, test::throwing_allocator<entt::entity>> set{GetParam()};

    test::throwing_allocator<entt::entity>::trigger_on_allocate = true;

    ASSERT_THROW(set.reserve(1u), test::throwing_allocator<entt::entity>::exception_type);
    ASSERT_EQ(set.capacity(), 0u);
    ASSERT_EQ(set.extent(), 0u);

    test::throwing_allocator<entt::entity>::trigger_on_allocate = true;

    ASSERT_THROW(set.push(entt::entity{0}), test::throwing_allocator<entt::entity>::exception_type);
    ASSERT_EQ(set.extent(), traits_type::page_size);
    ASSERT_EQ(set.capacity(), 0u);

    set.push(entt::entity{0});
    test::throwing_allocator<entt::entity>::trigger_on_allocate = true;

    ASSERT_THROW(set.reserve(2u), test::throwing_allocator<entt::entity>::exception_type);
    ASSERT_EQ(set.extent(), traits_type::page_size);
    ASSERT_TRUE(set.contains(entt::entity{0}));
    ASSERT_EQ(set.capacity(), 1u);

    test::throwing_allocator<entt::entity>::trigger_on_allocate = true;

    ASSERT_THROW(set.push(entt::entity{1}), test::throwing_allocator<entt::entity>::exception_type);
    ASSERT_EQ(set.extent(), traits_type::page_size);
    ASSERT_TRUE(set.contains(entt::entity{0}));
    ASSERT_FALSE(set.contains(entt::entity{1}));
    ASSERT_EQ(set.capacity(), 1u);

    entt::entity entity[2u]{entt::entity{1}, entt::entity{traits_type::page_size}};
    test::throwing_allocator<entt::entity>::trigger_after_allocate = true;

    ASSERT_THROW(set.push(std::begin(entity), std::end(entity)), test::throwing_allocator<entt::entity>::exception_type);
    ASSERT_EQ(set.extent(), 2 * traits_type::page_size);
    ASSERT_TRUE(set.contains(entt::entity{0}));
    ASSERT_TRUE(set.contains(entt::entity{1}));
    ASSERT_FALSE(set.contains(entt::entity{traits_type::page_size}));
    ASSERT_EQ(set.capacity(), 2u);
    ASSERT_EQ(set.size(), 2u);

    set.push(entity[1u]);

    ASSERT_TRUE(set.contains(entt::entity{traits_type::page_size}));
}
