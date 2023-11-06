#include <cstddef>
#include <iterator>
#include <type_traits>
#include <utility>
#include <gtest/gtest.h>
#include <entt/core/any.hpp>
#include <entt/core/type_info.hpp>
#include <entt/entity/component.hpp>
#include <entt/entity/entity.hpp>
#include <entt/entity/mixin.hpp>
#include <entt/entity/registry.hpp>
#include <entt/entity/storage.hpp>
#include "../common/non_default_constructible.h"
#include "../common/pointer_stable.h"
#include "../common/throwing_allocator.hpp"
#include "../common/throwing_type.hpp"

template<typename Registry>
void listener(std::size_t &counter, Registry &, typename Registry::entity_type) {
    ++counter;
}

template<typename Type>
struct SighMixin: testing::Test {
    using type = Type;
};

using SighMixinTypes = ::testing::Types<int, test::pointer_stable>;

TYPED_TEST_SUITE(SighMixin, SighMixinTypes, );

TEST(SighMixin, GenericType) {
    entt::entity entity[2u]{entt::entity{3}, entt::entity{42}};
    entt::sigh_mixin<entt::storage<int>> pool;
    entt::sparse_set &base = pool;
    entt::registry registry;

    std::size_t on_construct{};
    std::size_t on_destroy{};

    pool.bind(entt::forward_as_any(registry));

    ASSERT_EQ(pool.size(), 0u);

    pool.insert(entity, entity + 1u);
    pool.erase(entity[0u]);

    ASSERT_EQ(pool.size(), 0u);

    ASSERT_EQ(on_construct, 0u);
    ASSERT_EQ(on_destroy, 0u);

    pool.on_construct().connect<&listener<entt::registry>>(on_construct);
    pool.on_destroy().connect<&listener<entt::registry>>(on_destroy);

    ASSERT_NE(base.push(entity[0u]), base.end());

    pool.emplace(entity[1u]);

    ASSERT_EQ(on_construct, 2u);
    ASSERT_EQ(on_destroy, 0u);
    ASSERT_EQ(pool.size(), 2u);

    ASSERT_EQ(pool.get(entity[0u]), 0);
    ASSERT_EQ(pool.get(entity[1u]), 0);

    base.erase(entity[0u]);
    pool.erase(entity[1u]);

    ASSERT_EQ(on_construct, 2u);
    ASSERT_EQ(on_destroy, 2u);
    ASSERT_EQ(pool.size(), 0u);

    ASSERT_NE(base.push(std::begin(entity), std::end(entity)), base.end());

    ASSERT_EQ(pool.get(entity[0u]), 0);
    ASSERT_EQ(pool.get(entity[1u]), 0);
    ASSERT_EQ(pool.size(), 2u);

    base.erase(entity[1u]);

    ASSERT_EQ(on_construct, 4u);
    ASSERT_EQ(on_destroy, 3u);
    ASSERT_EQ(pool.size(), 1u);

    base.erase(entity[0u]);

    ASSERT_EQ(on_construct, 4u);
    ASSERT_EQ(on_destroy, 4u);
    ASSERT_EQ(pool.size(), 0u);

    pool.insert(std::begin(entity), std::end(entity), 3);

    ASSERT_EQ(on_construct, 6u);
    ASSERT_EQ(on_destroy, 4u);
    ASSERT_EQ(pool.size(), 2u);

    ASSERT_EQ(pool.get(entity[0u]), 3);
    ASSERT_EQ(pool.get(entity[1u]), 3);

    pool.erase(std::begin(entity), std::end(entity));

    ASSERT_EQ(on_construct, 6u);
    ASSERT_EQ(on_destroy, 6u);
    ASSERT_EQ(pool.size(), 0u);
}

TEST(SighMixin, StableType) {
    entt::entity entity[2u]{entt::entity{3}, entt::entity{42}};
    entt::sigh_mixin<entt::storage<test::pointer_stable>> pool;
    entt::sparse_set &base = pool;
    entt::registry registry;

    std::size_t on_construct{};
    std::size_t on_destroy{};

    pool.bind(entt::forward_as_any(registry));
    pool.on_construct().connect<&listener<entt::registry>>(on_construct);
    pool.on_destroy().connect<&listener<entt::registry>>(on_destroy);

    ASSERT_NE(base.push(entity[0u]), base.end());

    pool.emplace(entity[1u]);

    ASSERT_EQ(on_construct, 2u);
    ASSERT_EQ(on_destroy, 0u);
    ASSERT_EQ(pool.size(), 2u);

    ASSERT_EQ(pool.get(entity[0u]).value, 0);
    ASSERT_EQ(pool.get(entity[1u]).value, 0);

    base.erase(entity[0u]);
    pool.erase(entity[1u]);

    ASSERT_EQ(on_construct, 2u);
    ASSERT_EQ(on_destroy, 2u);
    ASSERT_EQ(pool.size(), 2u);

    ASSERT_NE(base.push(std::begin(entity), std::end(entity)), base.end());

    ASSERT_EQ(pool.get(entity[0u]).value, 0);
    ASSERT_EQ(pool.get(entity[1u]).value, 0);
    ASSERT_EQ(pool.size(), 4u);

    base.erase(entity[1u]);

    ASSERT_EQ(on_construct, 4u);
    ASSERT_EQ(on_destroy, 3u);
    ASSERT_EQ(pool.size(), 4u);

    base.erase(entity[0u]);

    ASSERT_EQ(on_construct, 4u);
    ASSERT_EQ(on_destroy, 4u);
    ASSERT_EQ(pool.size(), 4u);

    pool.insert(std::begin(entity), std::end(entity), test::pointer_stable{3});

    ASSERT_EQ(on_construct, 6u);
    ASSERT_EQ(on_destroy, 4u);
    ASSERT_EQ(pool.size(), 6u);

    ASSERT_EQ(pool.get(entity[0u]).value, 3);
    ASSERT_EQ(pool.get(entity[1u]).value, 3);

    pool.erase(std::begin(entity), std::end(entity));

    ASSERT_EQ(on_construct, 6u);
    ASSERT_EQ(on_destroy, 6u);
    ASSERT_EQ(pool.size(), 6u);
}

TEST(SighMixin, NonDefaultConstructibleType) {
    entt::entity entity[2u]{entt::entity{3}, entt::entity{42}};
    entt::sigh_mixin<entt::storage<test::non_default_constructible>> pool;
    entt::sparse_set &base = pool;
    entt::registry registry;

    std::size_t on_construct{};
    std::size_t on_destroy{};

    pool.bind(entt::forward_as_any(registry));
    pool.on_construct().connect<&listener<entt::registry>>(on_construct);
    pool.on_destroy().connect<&listener<entt::registry>>(on_destroy);

    ASSERT_EQ(base.push(entity[0u]), base.end());

    pool.emplace(entity[1u], 3);

    ASSERT_EQ(on_construct, 1u);
    ASSERT_EQ(on_destroy, 0u);
    ASSERT_EQ(pool.size(), 1u);

    ASSERT_FALSE(pool.contains(entity[0u]));
    ASSERT_EQ(pool.get(entity[1u]).value, 3);

    base.erase(entity[1u]);

    ASSERT_EQ(on_construct, 1u);
    ASSERT_EQ(on_destroy, 1u);
    ASSERT_EQ(pool.size(), 0u);

    ASSERT_EQ(base.push(std::begin(entity), std::end(entity)), base.end());

    ASSERT_FALSE(pool.contains(entity[0u]));
    ASSERT_FALSE(pool.contains(entity[1u]));
    ASSERT_EQ(pool.size(), 0u);

    pool.insert(std::begin(entity), std::end(entity), 3);

    ASSERT_EQ(on_construct, 3u);
    ASSERT_EQ(on_destroy, 1u);
    ASSERT_EQ(pool.size(), 2u);

    ASSERT_EQ(pool.get(entity[0u]).value, 3);
    ASSERT_EQ(pool.get(entity[1u]).value, 3);

    pool.erase(std::begin(entity), std::end(entity));

    ASSERT_EQ(on_construct, 3u);
    ASSERT_EQ(on_destroy, 3u);
    ASSERT_EQ(pool.size(), 0u);
}

TEST(SighMixin, VoidType) {
    entt::sigh_mixin<entt::storage<void>> pool;
    entt::registry registry;

    std::size_t on_construct{};
    std::size_t on_destroy{};

    pool.bind(entt::forward_as_any(registry));
    pool.on_construct().connect<&listener<entt::registry>>(on_construct);
    pool.on_destroy().connect<&listener<entt::registry>>(on_destroy);

    pool.emplace(entt::entity{99});

    ASSERT_EQ(pool.type(), entt::type_id<void>());
    ASSERT_TRUE(pool.contains(entt::entity{99}));

    entt::sigh_mixin<entt::storage<void>> other{std::move(pool)};

    ASSERT_FALSE(pool.contains(entt::entity{99}));
    ASSERT_TRUE(other.contains(entt::entity{99}));

    pool = std::move(other);

    ASSERT_TRUE(pool.contains(entt::entity{99}));
    ASSERT_FALSE(other.contains(entt::entity{99}));

    pool.clear();

    ASSERT_EQ(on_construct, 1u);
    ASSERT_EQ(on_destroy, 1u);
}

TEST(SighMixin, StorageEntity) {
    using traits_type = entt::entt_traits<entt::entity>;

    entt::sigh_mixin<entt::storage<entt::entity>> pool;
    entt::registry registry;

    std::size_t on_construct{};
    std::size_t on_destroy{};

    pool.bind(entt::forward_as_any(registry));
    pool.on_construct().connect<&listener<entt::registry>>(on_construct);
    pool.on_destroy().connect<&listener<entt::registry>>(on_destroy);

    pool.push(entt::entity{1});

    ASSERT_EQ(on_construct, 1u);
    ASSERT_EQ(on_destroy, 0u);
    ASSERT_EQ(pool.size(), 2u);
    ASSERT_EQ(pool.free_list(), 1u);

    pool.erase(entt::entity{1});

    ASSERT_EQ(on_construct, 1u);
    ASSERT_EQ(on_destroy, 1u);
    ASSERT_EQ(pool.size(), 2u);
    ASSERT_EQ(pool.free_list(), 0u);

    pool.push(traits_type::construct(0, 2));
    pool.push(traits_type::construct(2, 1));

    ASSERT_TRUE(pool.contains(traits_type::construct(0, 2)));
    ASSERT_TRUE(pool.contains(traits_type::construct(1, 1)));
    ASSERT_TRUE(pool.contains(traits_type::construct(2, 1)));

    ASSERT_EQ(on_construct, 3u);
    ASSERT_EQ(on_destroy, 1u);
    ASSERT_EQ(pool.size(), 3u);
    ASSERT_EQ(pool.free_list(), 2u);

    pool.clear();

    ASSERT_EQ(pool.size(), 0u);
    ASSERT_EQ(pool.free_list(), 0u);

    ASSERT_EQ(on_construct, 3u);
    ASSERT_EQ(on_destroy, 3u);

    pool.emplace();
    pool.emplace(entt::entity{0});

    entt::entity entity[1u]{};
    pool.insert(entity, entity + 1u);

    ASSERT_EQ(on_construct, 6u);
    ASSERT_EQ(on_destroy, 3u);
    ASSERT_EQ(pool.size(), 3u);
    ASSERT_EQ(pool.free_list(), 3u);

    pool.clear();

    ASSERT_EQ(pool.size(), 0u);
    ASSERT_EQ(pool.free_list(), 0u);
}

TYPED_TEST(SighMixin, Move) {
    using value_type = typename TestFixture::type;
    entt::sigh_mixin<entt::storage<value_type>> pool;
    entt::registry registry;

    std::size_t on_construct{};
    std::size_t on_destroy{};

    pool.bind(entt::forward_as_any(registry));
    pool.on_construct().template connect<&listener<entt::registry>>(on_construct);
    pool.on_destroy().template connect<&listener<entt::registry>>(on_destroy);

    pool.emplace(entt::entity{3}, 3);

    ASSERT_TRUE(std::is_move_constructible_v<decltype(pool)>);
    ASSERT_TRUE(std::is_move_assignable_v<decltype(pool)>);
    ASSERT_EQ(pool.type(), entt::type_id<value_type>());

    entt::sigh_mixin<entt::storage<value_type>> other{std::move(pool)};

    ASSERT_TRUE(pool.empty());
    ASSERT_FALSE(other.empty());
    ASSERT_EQ(other.type(), entt::type_id<value_type>());
    ASSERT_EQ(pool.at(0u), static_cast<entt::entity>(entt::null));
    ASSERT_EQ(other.at(0u), entt::entity{3});
    ASSERT_EQ(other.get(entt::entity{3}), value_type{3});

    pool = std::move(other);

    ASSERT_FALSE(pool.empty());
    ASSERT_TRUE(other.empty());
    ASSERT_EQ(pool.at(0u), entt::entity{3});
    ASSERT_EQ(pool.get(entt::entity{3}), value_type{3});
    ASSERT_EQ(other.at(0u), static_cast<entt::entity>(entt::null));

    other = entt::sigh_mixin<entt::storage<value_type>>{};
    other.bind(entt::forward_as_any(registry));

    other.emplace(entt::entity{42}, 42);
    other = std::move(pool);

    ASSERT_TRUE(pool.empty());
    ASSERT_FALSE(other.empty());
    ASSERT_EQ(pool.at(0u), static_cast<entt::entity>(entt::null));
    ASSERT_EQ(other.at(0u), entt::entity{3});
    ASSERT_EQ(other.get(entt::entity{3}), value_type{3});

    other.clear();

    ASSERT_EQ(on_construct, 1u);
    ASSERT_EQ(on_destroy, 1u);
}

TYPED_TEST(SighMixin, Swap) {
    using value_type = typename TestFixture::type;
    using traits_type = entt::component_traits<value_type>;
    entt::sigh_mixin<entt::storage<value_type>> pool;
    entt::sigh_mixin<entt::storage<value_type>> other;
    entt::registry registry;

    std::size_t on_construct{};
    std::size_t on_destroy{};

    pool.bind(entt::forward_as_any(registry));
    pool.on_construct().template connect<&listener<entt::registry>>(on_construct);
    pool.on_destroy().template connect<&listener<entt::registry>>(on_destroy);

    other.bind(entt::forward_as_any(registry));
    other.on_construct().template connect<&listener<entt::registry>>(on_construct);
    other.on_destroy().template connect<&listener<entt::registry>>(on_destroy);

    pool.emplace(entt::entity{42}, 41);

    other.emplace(entt::entity{9}, 8);
    other.emplace(entt::entity{3}, 2);
    other.erase(entt::entity{9});

    ASSERT_EQ(pool.size(), 1u);
    ASSERT_EQ(other.size(), 1u + traits_type::in_place_delete);

    pool.swap(other);

    ASSERT_EQ(pool.type(), entt::type_id<value_type>());
    ASSERT_EQ(other.type(), entt::type_id<value_type>());

    ASSERT_EQ(pool.size(), 1u + traits_type::in_place_delete);
    ASSERT_EQ(other.size(), 1u);

    ASSERT_EQ(pool.at(traits_type::in_place_delete), entt::entity{3});
    ASSERT_EQ(other.at(0u), entt::entity{42});

    ASSERT_EQ(pool.get(entt::entity{3}), value_type{2});
    ASSERT_EQ(other.get(entt::entity{42}), value_type{41});

    pool.clear();
    other.clear();

    ASSERT_EQ(on_construct, 3u);
    ASSERT_EQ(on_destroy, 3u);
}

TYPED_TEST(SighMixin, CustomAllocator) {
    using value_type = typename TestFixture::type;
    test::throwing_allocator<entt::entity> allocator{};
    entt::sigh_mixin<entt::basic_storage<value_type, entt::entity, test::throwing_allocator<value_type>>> pool{allocator};

    using registry_type = typename decltype(pool)::registry_type;
    registry_type registry;

    std::size_t on_construct{};
    std::size_t on_destroy{};

    pool.bind(entt::forward_as_any(registry));
    pool.on_construct().template connect<&listener<registry_type>>(on_construct);
    pool.on_destroy().template connect<&listener<registry_type>>(on_destroy);

    pool.reserve(1u);

    ASSERT_NE(pool.capacity(), 0u);

    pool.emplace(entt::entity{0});
    pool.emplace(entt::entity{1});

    decltype(pool) other{std::move(pool), allocator};

    ASSERT_TRUE(pool.empty());
    ASSERT_FALSE(other.empty());
    ASSERT_EQ(pool.capacity(), 0u);
    ASSERT_NE(other.capacity(), 0u);
    ASSERT_EQ(other.size(), 2u);

    pool = std::move(other);

    ASSERT_FALSE(pool.empty());
    ASSERT_TRUE(other.empty());
    ASSERT_EQ(other.capacity(), 0u);
    ASSERT_NE(pool.capacity(), 0u);
    ASSERT_EQ(pool.size(), 2u);

    pool.swap(other);
    pool = std::move(other);

    ASSERT_FALSE(pool.empty());
    ASSERT_TRUE(other.empty());
    ASSERT_EQ(other.capacity(), 0u);
    ASSERT_NE(pool.capacity(), 0u);
    ASSERT_EQ(pool.size(), 2u);

    pool.clear();

    ASSERT_NE(pool.capacity(), 0u);
    ASSERT_EQ(pool.size(), 0u);

    ASSERT_EQ(on_construct, 2u);
    ASSERT_EQ(on_destroy, 2u);
}

TYPED_TEST(SighMixin, ThrowingAllocator) {
    using value_type = typename TestFixture::type;
    using allocator_type = test::throwing_allocator<value_type>;
    entt::sigh_mixin<entt::basic_storage<value_type, entt::entity, test::throwing_allocator<value_type>>> pool{};
    typename std::decay_t<decltype(pool)>::base_type &base = pool;

    using registry_type = typename decltype(pool)::registry_type;
    registry_type registry;

    constexpr auto packed_page_size = entt::component_traits<typename decltype(pool)::value_type>::page_size;
    constexpr auto sparse_page_size = entt::entt_traits<typename decltype(pool)::entity_type>::page_size;

    std::size_t on_construct{};
    std::size_t on_destroy{};

    pool.bind(entt::forward_as_any(registry));
    pool.on_construct().template connect<&listener<registry_type>>(on_construct);
    pool.on_destroy().template connect<&listener<registry_type>>(on_destroy);

    allocator_type::trigger_on_allocate = true;

    ASSERT_THROW(pool.reserve(1u), typename allocator_type::exception_type);
    ASSERT_EQ(pool.capacity(), 0u);

    allocator_type::trigger_after_allocate = true;

    ASSERT_THROW(pool.reserve(2 * packed_page_size), typename allocator_type::exception_type);
    ASSERT_EQ(pool.capacity(), packed_page_size);

    pool.shrink_to_fit();

    ASSERT_EQ(pool.capacity(), 0u);

    test::throwing_allocator<entt::entity>::trigger_on_allocate = true;

    ASSERT_THROW(pool.emplace(entt::entity{0}, 0), test::throwing_allocator<entt::entity>::exception_type);
    ASSERT_FALSE(pool.contains(entt::entity{0}));
    ASSERT_TRUE(pool.empty());

    test::throwing_allocator<entt::entity>::trigger_on_allocate = true;

    ASSERT_THROW(base.push(entt::entity{0}), test::throwing_allocator<entt::entity>::exception_type);
    ASSERT_FALSE(base.contains(entt::entity{0}));
    ASSERT_TRUE(base.empty());

    allocator_type::trigger_on_allocate = true;

    ASSERT_THROW(pool.emplace(entt::entity{0}, 0), typename allocator_type::exception_type);
    ASSERT_FALSE(pool.contains(entt::entity{0}));
    ASSERT_NO_FATAL_FAILURE(pool.compact());
    ASSERT_TRUE(pool.empty());

    pool.emplace(entt::entity{0}, 0);
    const entt::entity entity[2u]{entt::entity{1}, entt::entity{sparse_page_size}};
    test::throwing_allocator<entt::entity>::trigger_after_allocate = true;

    ASSERT_THROW(pool.insert(std::begin(entity), std::end(entity), value_type{0}), test::throwing_allocator<entt::entity>::exception_type);
    ASSERT_TRUE(pool.contains(entt::entity{1}));
    ASSERT_FALSE(pool.contains(entt::entity{sparse_page_size}));

    pool.erase(entt::entity{1});
    const value_type components[2u]{value_type{1}, value_type{sparse_page_size}};
    test::throwing_allocator<entt::entity>::trigger_on_allocate = true;
    pool.compact();

    ASSERT_THROW(pool.insert(std::begin(entity), std::end(entity), std::begin(components)), test::throwing_allocator<entt::entity>::exception_type);
    ASSERT_TRUE(pool.contains(entt::entity{1}));
    ASSERT_FALSE(pool.contains(entt::entity{sparse_page_size}));

    ASSERT_EQ(on_construct, 1u);
    ASSERT_EQ(on_destroy, 1u);
}

TEST(SighMixin, ThrowingComponent) {
    entt::sigh_mixin<entt::storage<test::throwing_type>> pool;
    using registry_type = typename decltype(pool)::registry_type;
    registry_type registry;

    std::size_t on_construct{};
    std::size_t on_destroy{};

    pool.bind(entt::forward_as_any(registry));
    pool.on_construct().connect<&listener<registry_type>>(on_construct);
    pool.on_destroy().connect<&listener<registry_type>>(on_destroy);

    const entt::entity entity[2u]{entity[0u], entt::entity{1}};
    const test::throwing_type value[2u]{test::throwing_type::trigger_on_value, 1};

    // strong exception safety
    ASSERT_THROW(pool.emplace(entity[0u], value[0u]), typename test::throwing_type::exception_type);
    ASSERT_TRUE(pool.empty());

    // basic exception safety
    ASSERT_THROW(pool.insert(std::begin(entity), std::end(entity), value[0u]), typename test::throwing_type::exception_type);
    ASSERT_EQ(pool.size(), 0u);
    ASSERT_FALSE(pool.contains(entity[1u]));

    // basic exception safety
    ASSERT_THROW(pool.insert(std::begin(entity), std::end(entity), std::begin(value)), typename test::throwing_type::exception_type);
    ASSERT_EQ(pool.size(), 0u);
    ASSERT_FALSE(pool.contains(entity[1u]));

    // basic exception safety
    ASSERT_THROW(pool.insert(std::rbegin(entity), std::rend(entity), std::rbegin(value)), typename test::throwing_type::exception_type);
    ASSERT_EQ(pool.size(), 1u);
    ASSERT_TRUE(pool.contains(entity[1u]));
    ASSERT_EQ(pool.get(entity[1u]), value[1u]);

    pool.clear();
    pool.emplace(entity[1u], value[1u].get());
    pool.emplace(entity[0u], value[0u].get());

    // basic exception safety
    ASSERT_THROW(pool.erase(entity[1u]), typename test::throwing_type::exception_type);
    ASSERT_EQ(pool.size(), 2u);
    ASSERT_TRUE(pool.contains(entity[0u]));
    ASSERT_TRUE(pool.contains(entity[1u]));
    ASSERT_EQ(pool.at(0u), entity[1u]);
    ASSERT_EQ(pool.at(1u), entity[0u]);
    ASSERT_EQ(pool.get(entity[0u]), value[0u]);
    // the element may have been moved but it's still there
    ASSERT_EQ(pool.get(entity[1u]), test::throwing_type::moved_from_value);

    pool.get(entity[0u]).set(value[1u].get());
    pool.erase(entity[1u]);

    ASSERT_EQ(pool.size(), 1u);
    ASSERT_TRUE(pool.contains(entity[0u]));
    ASSERT_FALSE(pool.contains(entity[1u]));
    ASSERT_EQ(pool.at(0u), entity[0u]);
    ASSERT_EQ(pool.get(entity[0u]), value[1u]);

    ASSERT_EQ(on_construct, 2u);
    ASSERT_EQ(on_destroy, 3u);
}
