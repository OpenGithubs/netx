#ifndef ENTT_ENTITY_GROUP_HPP
#define ENTT_ENTITY_GROUP_HPP

#include <tuple>
#include <type_traits>
#include <utility>
#include "../config/config.h"
#include "../core/fwd.hpp"
#include "../core/iterator.hpp"
#include "../core/type_info.hpp"
#include "../core/type_traits.hpp"
#include "entity.hpp"
#include "fwd.hpp"
#include "sparse_set.hpp"
#include "storage.hpp"

namespace entt {

/**
 * @cond TURN_OFF_DOXYGEN
 * Internal details not to be documented.
 */

namespace internal {

template<typename, typename, typename>
class extended_group_iterator;

template<typename It, typename... Owned, typename... Get>
class extended_group_iterator<It, owned_t<Owned...>, get_t<Get...>> {
    template<typename Type>
    auto index_to_element([[maybe_unused]] Type &cpool) const {
        if constexpr(Type::traits_type::page_size == 0u) {
            return std::make_tuple();
        } else {
            return std::forward_as_tuple(cpool.rbegin()[it.index()]);
        }
    }

public:
    using iterator_type = It;
    using difference_type = std::ptrdiff_t;
    using value_type = decltype(std::tuple_cat(std::make_tuple(*std::declval<It>()), std::declval<Owned>().get_as_tuple({})..., std::declval<Get>().get_as_tuple({})...));
    using pointer = input_iterator_pointer<value_type>;
    using reference = value_type;
    using iterator_category = std::input_iterator_tag;

    constexpr extended_group_iterator()
        : it{},
          pools{} {}

    extended_group_iterator(It from, const std::tuple<Owned *..., Get *...> &cpools)
        : it{from},
          pools{cpools} {}

    extended_group_iterator &operator++() noexcept {
        return ++it, *this;
    }

    extended_group_iterator operator++(int) noexcept {
        extended_group_iterator orig = *this;
        return ++(*this), orig;
    }

    [[nodiscard]] reference operator*() const noexcept {
        return std::tuple_cat(std::make_tuple(*it), index_to_element(*std::get<Owned *>(pools))..., std::get<Get *>(pools)->get_as_tuple(*it)...);
    }

    [[nodiscard]] pointer operator->() const noexcept {
        return operator*();
    }

    [[nodiscard]] constexpr iterator_type base() const noexcept {
        return it;
    }

    template<typename... Lhs, typename... Rhs>
    friend constexpr bool operator==(const extended_group_iterator<Lhs...> &, const extended_group_iterator<Rhs...> &) noexcept;

private:
    It it;
    std::tuple<Owned *..., Get *...> pools;
};

template<typename... Lhs, typename... Rhs>
[[nodiscard]] constexpr bool operator==(const extended_group_iterator<Lhs...> &lhs, const extended_group_iterator<Rhs...> &rhs) noexcept {
    return lhs.it == rhs.it;
}

template<typename... Lhs, typename... Rhs>
[[nodiscard]] constexpr bool operator!=(const extended_group_iterator<Lhs...> &lhs, const extended_group_iterator<Rhs...> &rhs) noexcept {
    return !(lhs == rhs);
}

struct basic_group_handler {
    const std::size_t size;
    bool (*const owned)(const id_type) noexcept;
    bool (*const get)(const id_type) noexcept;
    bool (*const exclude)(const id_type) noexcept;
};

template<typename, typename, typename>
class group_handler;

template<typename... Owned, typename... Get, typename... Exclude>
class group_handler<owned_t<Owned...>, get_t<Get...>, exclude_t<Exclude...>> final: public basic_group_handler {
    // nasty workaround for an issue with the toolset v141 that doesn't accept a fold expression here
    static_assert(!std::disjunction_v<std::bool_constant<Owned::traits_type::in_place_delete>...>, "Groups do not support in-place delete");
    static_assert(!std::disjunction_v<std::is_const<Owned>..., std::is_const<Get>..., std::is_const<Exclude>...>, "Const storage type not allowed");

    using underlying_type = std::common_type_t<typename Owned::entity_type..., typename Get::entity_type..., typename Exclude::entity_type...>;

    template<std::size_t... Index>
    void swap_elements(std::index_sequence<Index...>, const std::size_t pos, const underlying_type entt) {
        (std::get<Index>(pools)->swap_elements(std::get<Index>(pools)->data()[pos], entt), ...);
    }

public:
    using entity_type = underlying_type;

    group_handler(Owned &...opool, Get &...gpool, Exclude &...epool)
        : basic_group_handler{
            sizeof...(Owned) + sizeof...(Get) + sizeof...(Exclude),
            +[](const id_type ctype) noexcept { return ((ctype == entt::type_hash<typename Owned::value_type>::value()) || ...); },
            +[]([[maybe_unused]] const id_type ctype) noexcept { return ((ctype == entt::type_hash<typename Get::value_type>::value()) || ...); },
            +[]([[maybe_unused]] const id_type ctype) noexcept { return ((ctype == entt::type_hash<typename Exclude::value_type>::value()) || ...); }},
          pools{&opool..., &gpool...}, filter{&epool...}, len{} {}

    void push_on_construct(const entity_type entt) {
        if(std::apply([entt](auto *...cpool) { return (cpool->contains(entt) && ...); }, pools)
           && std::apply([entt](auto *...cpool) { return (!cpool->contains(entt) && ...); }, filter)
           && !(std::get<0>(pools)->index(entt) < len)) {
            swap_elements(std::index_sequence_for<Owned...>{}, len++, entt);
        }
    }

    void push_on_destroy(const entity_type entt) {
        if(std::apply([entt](auto *...cpool) { return (cpool->contains(entt) && ...); }, pools)
           && std::apply([entt](auto *...cpool) { return (0u + ... + cpool->contains(entt)) == 1u; }, filter)
           && !(std::get<0>(pools)->index(entt) < len)) {
            swap_elements(std::index_sequence_for<Owned...>{}, len++, entt);
        }
    }

    void remove_if(const entity_type entt) {
        if(std::get<0>(pools)->contains(entt) && (std::get<0>(pools)->index(entt) < len)) {
            swap_elements(std::index_sequence_for<Owned...>{}, --len, entt);
        }
    }

    [[nodiscard]] std::size_t length() const noexcept {
        return len;
    }

    template<typename Type>
    Type pools_as() const noexcept {
        return pools;
    }

    template<typename Type>
    Type filter_as() const noexcept {
        return filter;
    }

private:
    std::tuple<Owned *..., Get *...> pools;
    std::tuple<Exclude *...> filter;
    std::size_t len;
};

template<typename... Get, typename... Exclude>
class group_handler<owned_t<>, get_t<Get...>, exclude_t<Exclude...>>: public basic_group_handler {
    // nasty workaround for an issue with the toolset v141 that doesn't accept a fold expression here
    static_assert(!std::disjunction_v<std::is_const<Get>..., std::is_const<Exclude>...>, "Const storage type not allowed");

public:
    using basic_common_type = std::common_type_t<typename Get::base_type..., typename Exclude::base_type...>;
    using entity_type = typename basic_common_type::entity_type;

    template<typename Alloc>
    group_handler(const Alloc &alloc, Get &...gpool, Exclude &...epool)
        : basic_group_handler{
            sizeof...(Get) + sizeof...(Exclude),
            +[](const id_type) noexcept { return false; },
            +[](const id_type ctype) noexcept { return ((ctype == entt::type_hash<typename Get::value_type>::value()) || ...); },
            +[]([[maybe_unused]] const id_type ctype) noexcept { return ((ctype == entt::type_hash<typename Exclude::value_type>::value()) || ...); }},
          pools{&gpool...}, filter{&epool...}, elem{alloc} {}

    void push_on_construct(const entity_type entt) {
        if(std::apply([entt](auto *...cpool) { return (cpool->contains(entt) && ...); }, pools)
           && std::apply([entt](auto *...cpool) { return (!cpool->contains(entt) && ...); }, filter)
           && !elem.contains(entt)) {
            elem.push(entt);
        }
    }

    void push_on_destroy(const entity_type entt) {
        if(std::apply([entt](auto *...cpool) { return (cpool->contains(entt) && ...); }, pools)
           && std::apply([entt](auto *...cpool) { return (0u + ... + cpool->contains(entt)) == 1u; }, filter)
           && !elem.contains(entt)) {
            elem.push(entt);
        }
    }

    void remove_if(const entity_type entt) {
        elem.remove(entt);
    }

    basic_common_type &group() noexcept {
        return elem;
    }

    const basic_common_type &group() const noexcept {
        return elem;
    }

    template<typename Type>
    Type pools_as() const noexcept {
        return pools;
    }

    template<typename Type>
    Type filter_as() const noexcept {
        return filter;
    }

private:
    std::tuple<Get *...> pools;
    std::tuple<Exclude *...> filter;
    basic_common_type elem;
};

} // namespace internal

/**
 * Internal details not to be documented.
 * @endcond
 */

/**
 * @brief Group.
 *
 * Primary template isn't defined on purpose. All the specializations give a
 * compile-time error, but for a few reasonable cases.
 */
template<typename, typename, typename>
class basic_group;

/**
 * @brief Non-owning group.
 *
 * A non-owning group returns all entities and only the entities that are at
 * least in the given storage. Moreover, it's guaranteed that the entity list is
 * tightly packed in memory for fast iterations.
 *
 * @b Important
 *
 * Iterators aren't invalidated if:
 *
 * * New elements are added to the storage.
 * * The entity currently pointed is modified (for example, components are added
 *   or removed from it).
 * * The entity currently pointed is destroyed.
 *
 * In all other cases, modifying the pools iterated by the group in any way
 * invalidates all the iterators and using them results in undefined behavior.
 *
 * @tparam Get Types of storage _observed_ by the group.
 * @tparam Exclude Types of storage used to filter the group.
 */
template<typename... Get, typename... Exclude>
class basic_group<owned_t<>, get_t<Get...>, exclude_t<Exclude...>> {
    using underlying_type = std::common_type_t<typename Get::entity_type..., typename Exclude::entity_type...>;
    using basic_common_type = std::common_type_t<typename Get::base_type..., typename Exclude::base_type...>;

    template<typename Type>
    static constexpr std::size_t index_of = type_list_index_v<std::remove_const_t<Type>, type_list<typename Get::value_type..., typename Exclude::value_type...>>;

    auto pools() const noexcept {
        return descriptor->template pools_as<std::tuple<Get *...>>();
    }

    auto filter() const noexcept {
        return descriptor->template filter_as<std::tuple<Exclude *...>>();
    }

public:
    /*! @brief Underlying entity identifier. */
    using entity_type = underlying_type;
    /*! @brief Unsigned integer type. */
    using size_type = std::size_t;
    /*! @brief Common type among all storage types. */
    using base_type = basic_common_type;
    /*! @brief Random access iterator type. */
    using iterator = typename base_type::iterator;
    /*! @brief Reversed iterator type. */
    using reverse_iterator = typename base_type::reverse_iterator;
    /*! @brief Iterable group type. */
    using iterable = iterable_adaptor<internal::extended_group_iterator<iterator, owned_t<>, get_t<Get...>>>;
    /*! @brief Group handler type. */
    using handler = internal::group_handler<owned_t<>, get_t<std::remove_const_t<Get>...>, exclude_t<std::remove_const_t<Exclude>...>>;

    /*! @brief Default constructor to use to create empty, invalid groups. */
    basic_group() noexcept
        : descriptor{} {}

    /**
     * @brief Constructs a group from a set of storage classes.
     * @param ref A reference to a group handler.
     */
    basic_group(handler &ref) noexcept
        : descriptor{&ref} {}

    /**
     * @brief Returns the leading storage of a group.
     * @return The leading storage of the group.
     */
    [[nodiscard]] const base_type &handle() const noexcept {
        return descriptor->group();
    }

    /**
     * @brief Returns the storage for a given component type.
     * @tparam Type Type of component of which to return the storage.
     * @return The storage for the given component type.
     */
    template<typename Type>
    [[nodiscard]] decltype(auto) storage() const noexcept {
        return storage<index_of<Type>>();
    }

    /**
     * @brief Returns the storage for a given index.
     * @tparam Index Index of the storage to return.
     * @return The storage for the given index.
     */
    template<std::size_t Index>
    [[nodiscard]] decltype(auto) storage() const noexcept {
        constexpr auto offset = sizeof...(Get);

        if constexpr(Index < offset) {
            return *std::get<Index>(pools());
        } else {
            return *std::get<Index - offset>(filter());
        }
    }

    /**
     * @brief Returns the number of entities that are part of the group.
     * @return Number of entities that are part of the group.
     */
    [[nodiscard]] size_type size() const noexcept {
        return *this ? descriptor->group().size() : size_type{};
    }

    /**
     * @brief Returns the number of elements that a group has currently
     * allocated space for.
     * @return Capacity of the group.
     */
    [[nodiscard]] size_type capacity() const noexcept {
        return *this ? descriptor->group().capacity() : size_type{};
    }

    /*! @brief Requests the removal of unused capacity. */
    void shrink_to_fit() {
        if(*this) {
            descriptor->group().shrink_to_fit();
        }
    }

    /**
     * @brief Checks whether a group is empty.
     * @return True if the group is empty, false otherwise.
     */
    [[nodiscard]] bool empty() const noexcept {
        return !*this || descriptor->group().empty();
    }

    /**
     * @brief Returns an iterator to the first entity of the group.
     *
     * The returned iterator points to the first entity of the group. If the
     * group is empty, the returned iterator will be equal to `end()`.
     *
     * @return An iterator to the first entity of the group.
     */
    [[nodiscard]] iterator begin() const noexcept {
        return *this ? descriptor->group().begin() : iterator{};
    }

    /**
     * @brief Returns an iterator that is past the last entity of the group.
     *
     * The returned iterator points to the entity following the last entity of
     * the group. Attempting to dereference the returned iterator results in
     * undefined behavior.
     *
     * @return An iterator to the entity following the last entity of the
     * group.
     */
    [[nodiscard]] iterator end() const noexcept {
        return *this ? descriptor->group().end() : iterator{};
    }

    /**
     * @brief Returns an iterator to the first entity of the reversed group.
     *
     * The returned iterator points to the first entity of the reversed group.
     * If the group is empty, the returned iterator will be equal to `rend()`.
     *
     * @return An iterator to the first entity of the reversed group.
     */
    [[nodiscard]] reverse_iterator rbegin() const noexcept {
        return *this ? descriptor->group().rbegin() : reverse_iterator{};
    }

    /**
     * @brief Returns an iterator that is past the last entity of the reversed
     * group.
     *
     * The returned iterator points to the entity following the last entity of
     * the reversed group. Attempting to dereference the returned iterator
     * results in undefined behavior.
     *
     * @return An iterator to the entity following the last entity of the
     * reversed group.
     */
    [[nodiscard]] reverse_iterator rend() const noexcept {
        return *this ? descriptor->group().rend() : reverse_iterator{};
    }

    /**
     * @brief Returns the first entity of the group, if any.
     * @return The first entity of the group if one exists, the null entity
     * otherwise.
     */
    [[nodiscard]] entity_type front() const noexcept {
        const auto it = begin();
        return it != end() ? *it : null;
    }

    /**
     * @brief Returns the last entity of the group, if any.
     * @return The last entity of the group if one exists, the null entity
     * otherwise.
     */
    [[nodiscard]] entity_type back() const noexcept {
        const auto it = rbegin();
        return it != rend() ? *it : null;
    }

    /**
     * @brief Finds an entity.
     * @param entt A valid identifier.
     * @return An iterator to the given entity if it's found, past the end
     * iterator otherwise.
     */
    [[nodiscard]] iterator find(const entity_type entt) const noexcept {
        const auto it = *this ? descriptor->group().find(entt) : iterator{};
        return it != end() && *it == entt ? it : end();
    }

    /**
     * @brief Returns the identifier that occupies the given position.
     * @param pos Position of the element to return.
     * @return The identifier that occupies the given position.
     */
    [[nodiscard]] entity_type operator[](const size_type pos) const {
        return begin()[pos];
    }

    /**
     * @brief Checks if a group is properly initialized.
     * @return True if the group is properly initialized, false otherwise.
     */
    [[nodiscard]] explicit operator bool() const noexcept {
        return descriptor != nullptr;
    }

    /**
     * @brief Checks if a group contains an entity.
     * @param entt A valid identifier.
     * @return True if the group contains the given entity, false otherwise.
     */
    [[nodiscard]] bool contains(const entity_type entt) const noexcept {
        return *this && descriptor->group().contains(entt);
    }

    /**
     * @brief Returns the components assigned to the given entity.
     *
     * @warning
     * Attempting to use an entity that doesn't belong to the group results in
     * undefined behavior.
     *
     * @tparam Type Type of the component to get.
     * @tparam Other Other types of components to get.
     * @param entt A valid identifier.
     * @return The components assigned to the entity.
     */
    template<typename Type, typename... Other>
    [[nodiscard]] decltype(auto) get(const entity_type entt) const {
        return get<index_of<Type>, index_of<Other>...>(entt);
    }

    /**
     * @brief Returns the components assigned to the given entity.
     *
     * @warning
     * Attempting to use an entity that doesn't belong to the groups results in
     * undefined behavior.
     *
     * @tparam Index Indexes of the components to get.
     * @param entt A valid identifier.
     * @return The components assigned to the entity.
     */
    template<std::size_t... Index>
    [[nodiscard]] decltype(auto) get(const entity_type entt) const {
        const auto cpools = pools();

        if constexpr(sizeof...(Index) == 0) {
            return std::apply([entt](auto *...curr) { return std::tuple_cat(curr->get_as_tuple(entt)...); }, cpools);
        } else if constexpr(sizeof...(Index) == 1) {
            return (std::get<Index>(cpools)->get(entt), ...);
        } else {
            return std::tuple_cat(std::get<Index>(cpools)->get_as_tuple(entt)...);
        }
    }

    /**
     * @brief Iterates entities and components and applies the given function
     * object to them.
     *
     * The function object is invoked for each entity. It is provided with the
     * entity itself and a set of references to non-empty components. The
     * _constness_ of the components is as requested.<br/>
     * The signature of the function must be equivalent to one of the following
     * forms:
     *
     * @code{.cpp}
     * void(const entity_type, Type &...);
     * void(Type &...);
     * @endcode
     *
     * @note
     * Empty types aren't explicitly instantiated and therefore they are never
     * returned during iterations.
     *
     * @tparam Func Type of the function object to invoke.
     * @param func A valid function object.
     */
    template<typename Func>
    void each(Func func) const {
        for(const auto entt: *this) {
            if constexpr(is_applicable_v<Func, decltype(std::tuple_cat(std::tuple<entity_type>{}, std::declval<basic_group>().get({})))>) {
                std::apply(func, std::tuple_cat(std::make_tuple(entt), get(entt)));
            } else {
                std::apply(func, get(entt));
            }
        }
    }

    /**
     * @brief Returns an iterable object to use to _visit_ a group.
     *
     * The iterable object returns tuples that contain the current entity and a
     * set of references to its non-empty components. The _constness_ of the
     * components is as requested.
     *
     * @note
     * Empty types aren't explicitly instantiated and therefore they are never
     * returned during iterations.
     *
     * @return An iterable object to use to _visit_ the group.
     */
    [[nodiscard]] iterable each() const noexcept {
        const auto cpools = pools();
        return iterable{{begin(), cpools}, {end(), cpools}};
    }

    /**
     * @brief Sort a group according to the given comparison function.
     *
     * The comparison function object must return `true` if the first element
     * is _less_ than the second one, `false` otherwise. The signature of the
     * comparison function should be equivalent to one of the following:
     *
     * @code{.cpp}
     * bool(std::tuple<Type &...>, std::tuple<Type &...>);
     * bool(const Type &..., const Type &...);
     * bool(const Entity, const Entity);
     * @endcode
     *
     * Where `Type` are such that they are iterated by the group.<br/>
     * Moreover, the comparison function object shall induce a
     * _strict weak ordering_ on the values.
     *
     * The sort function object must offer a member function template
     * `operator()` that accepts three arguments:
     *
     * * An iterator to the first element of the range to sort.
     * * An iterator past the last element of the range to sort.
     * * A comparison function to use to compare the elements.
     *
     * @tparam Type Optional types of components to compare.
     * @tparam Compare Type of comparison function object.
     * @tparam Sort Type of sort function object.
     * @tparam Args Types of arguments to forward to the sort function object.
     * @param compare A valid comparison function object.
     * @param algo A valid sort function object.
     * @param args Arguments to forward to the sort function object, if any.
     */
    template<typename... Type, typename Compare, typename Sort = std_sort, typename... Args>
    void sort(Compare compare, Sort algo = Sort{}, Args &&...args) {
        if(*this) {
            if constexpr(sizeof...(Type) == 0) {
                static_assert(std::is_invocable_v<Compare, const entity_type, const entity_type>, "Invalid comparison function");
                descriptor->group().sort(std::move(compare), std::move(algo), std::forward<Args>(args)...);
            } else {
                auto comp = [&compare, cpools = pools()](const entity_type lhs, const entity_type rhs) {
                    if constexpr(sizeof...(Type) == 1) {
                        return compare((std::get<index_of<Type>>(cpools)->get(lhs), ...), (std::get<index_of<Type>>(cpools)->get(rhs), ...));
                    } else {
                        return compare(std::forward_as_tuple(std::get<index_of<Type>>(cpools)->get(lhs)...), std::forward_as_tuple(std::get<index_of<Type>>(cpools)->get(rhs)...));
                    }
                };

                descriptor->group().sort(std::move(comp), std::move(algo), std::forward<Args>(args)...);
            }
        }
    }

    /**
     * @brief Sort the shared pool of entities according to a given storage.
     *
     * The shared pool of entities and thus its order is affected by the changes
     * to each and every pool that it tracks.
     *
     * @param other The storage to use to impose the order.
     */
    void sort_as(const base_type &other) const {
        if(*this) {
            descriptor->group().sort_as(other);
        }
    }

private:
    handler *descriptor;
};

/**
 * @brief Owning group.
 *
 * Owning groups returns all entities and only the entities that are at
 * least in the given storage. Moreover:
 *
 * * It's guaranteed that the entity list is tightly packed in memory for fast
 *   iterations.
 * * It's guaranteed that all components in the owned storage are tightly packed
 *   in memory for even faster iterations and to allow direct access.
 * * They stay true to the order of the owned storage and all instances have the
 *   same order in memory.
 *
 * The more types of storage are owned, the faster it is to iterate a group.
 *
 * @b Important
 *
 * Iterators aren't invalidated if:
 *
 * * New elements are added to the storage.
 * * The entity currently pointed is modified (for example, components are added
 *   or removed from it).
 * * The entity currently pointed is destroyed.
 *
 * In all other cases, modifying the pools iterated by the group in any way
 * invalidates all the iterators and using them results in undefined behavior.
 *
 * @tparam Owned Types of storage _owned_ by the group.
 * @tparam Get Types of storage _observed_ by the group.
 * @tparam Exclude Types of storage used to filter the group.
 */
template<typename... Owned, typename... Get, typename... Exclude>
class basic_group<owned_t<Owned...>, get_t<Get...>, exclude_t<Exclude...>> {
    using underlying_type = std::common_type_t<typename Owned::entity_type..., typename Get::entity_type..., typename Exclude::entity_type...>;
    using basic_common_type = std::common_type_t<typename Owned::base_type..., typename Get::base_type..., typename Exclude::base_type...>;

    template<typename Type>
    static constexpr std::size_t index_of = type_list_index_v<std::remove_const_t<Type>, type_list<typename Owned::value_type..., typename Get::value_type..., typename Exclude::value_type...>>;

    auto pools() const noexcept {
        return descriptor->template pools_as<std::tuple<Owned *..., Get *...>>();
    }

    auto filter() const noexcept {
        return descriptor->template filter_as<std::tuple<Exclude *...>>();
    }

public:
    /*! @brief Underlying entity identifier. */
    using entity_type = underlying_type;
    /*! @brief Unsigned integer type. */
    using size_type = std::size_t;
    /*! @brief Common type among all storage types. */
    using base_type = basic_common_type;
    /*! @brief Random access iterator type. */
    using iterator = typename base_type::iterator;
    /*! @brief Reversed iterator type. */
    using reverse_iterator = typename base_type::reverse_iterator;
    /*! @brief Iterable group type. */
    using iterable = iterable_adaptor<internal::extended_group_iterator<iterator, owned_t<Owned...>, get_t<Get...>>>;
    /*! @brief Group handler type. */
    using handler = internal::group_handler<owned_t<std::remove_const_t<Owned>...>, get_t<std::remove_const_t<Get>...>, exclude_t<std::remove_const_t<Exclude>...>>;

    /*! @brief Default constructor to use to create empty, invalid groups. */
    basic_group() noexcept
        : descriptor{} {}

    /**
     * @brief Constructs a group from a set of storage classes.
     * @param ref A reference to a group handler.
     */
    basic_group(handler &ref) noexcept
        : descriptor{&ref} {}

    /**
     * @brief Returns the leading storage of a group.
     * @return The leading storage of the group.
     */
    [[nodiscard]] const base_type &handle() const noexcept {
        return storage<0>();
    }

    /**
     * @brief Returns the storage for a given component type.
     * @tparam Type Type of component of which to return the storage.
     * @return The storage for the given component type.
     */
    template<typename Type>
    [[nodiscard]] decltype(auto) storage() const noexcept {
        return storage<index_of<Type>>();
    }

    /**
     * @brief Returns the storage for a given index.
     * @tparam Index Index of the storage to return.
     * @return The storage for the given index.
     */
    template<std::size_t Index>
    [[nodiscard]] decltype(auto) storage() const noexcept {
        constexpr auto offset = sizeof...(Owned) + sizeof...(Get);

        if constexpr(Index < offset) {
            return *std::get<Index>(pools());
        } else {
            return *std::get<Index - offset>(filter());
        }
    }

    /**
     * @brief Returns the number of entities that that are part of the group.
     * @return Number of entities that that are part of the group.
     */
    [[nodiscard]] size_type size() const noexcept {
        return *this ? descriptor->length() : size_type{};
    }

    /**
     * @brief Checks whether a group is empty.
     * @return True if the group is empty, false otherwise.
     */
    [[nodiscard]] bool empty() const noexcept {
        return !*this || !descriptor->length();
    }

    /**
     * @brief Returns an iterator to the first entity of the group.
     *
     * The returned iterator points to the first entity of the group. If the
     * group is empty, the returned iterator will be equal to `end()`.
     *
     * @return An iterator to the first entity of the group.
     */
    [[nodiscard]] iterator begin() const noexcept {
        return *this ? (handle().base_type::end() - descriptor->length()) : iterator{};
    }

    /**
     * @brief Returns an iterator that is past the last entity of the group.
     *
     * The returned iterator points to the entity following the last entity of
     * the group. Attempting to dereference the returned iterator results in
     * undefined behavior.
     *
     * @return An iterator to the entity following the last entity of the
     * group.
     */
    [[nodiscard]] iterator end() const noexcept {
        return *this ? handle().base_type::end() : iterator{};
    }

    /**
     * @brief Returns an iterator to the first entity of the reversed group.
     *
     * The returned iterator points to the first entity of the reversed group.
     * If the group is empty, the returned iterator will be equal to `rend()`.
     *
     * @return An iterator to the first entity of the reversed group.
     */
    [[nodiscard]] reverse_iterator rbegin() const noexcept {
        return *this ? handle().base_type::rbegin() : reverse_iterator{};
    }

    /**
     * @brief Returns an iterator that is past the last entity of the reversed
     * group.
     *
     * The returned iterator points to the entity following the last entity of
     * the reversed group. Attempting to dereference the returned iterator
     * results in undefined behavior.
     *
     * @return An iterator to the entity following the last entity of the
     * reversed group.
     */
    [[nodiscard]] reverse_iterator rend() const noexcept {
        return *this ? (handle().base_type::rbegin() + descriptor->length()) : reverse_iterator{};
    }

    /**
     * @brief Returns the first entity of the group, if any.
     * @return The first entity of the group if one exists, the null entity
     * otherwise.
     */
    [[nodiscard]] entity_type front() const noexcept {
        const auto it = begin();
        return it != end() ? *it : null;
    }

    /**
     * @brief Returns the last entity of the group, if any.
     * @return The last entity of the group if one exists, the null entity
     * otherwise.
     */
    [[nodiscard]] entity_type back() const noexcept {
        const auto it = rbegin();
        return it != rend() ? *it : null;
    }

    /**
     * @brief Finds an entity.
     * @param entt A valid identifier.
     * @return An iterator to the given entity if it's found, past the end
     * iterator otherwise.
     */
    [[nodiscard]] iterator find(const entity_type entt) const noexcept {
        const auto it = *this ? handle().find(entt) : iterator{};
        return it != end() && it >= begin() && *it == entt ? it : end();
    }

    /**
     * @brief Returns the identifier that occupies the given position.
     * @param pos Position of the element to return.
     * @return The identifier that occupies the given position.
     */
    [[nodiscard]] entity_type operator[](const size_type pos) const {
        return begin()[pos];
    }

    /**
     * @brief Checks if a group is properly initialized.
     * @return True if the group is properly initialized, false otherwise.
     */
    [[nodiscard]] explicit operator bool() const noexcept {
        return descriptor != nullptr;
    }

    /**
     * @brief Checks if a group contains an entity.
     * @param entt A valid identifier.
     * @return True if the group contains the given entity, false otherwise.
     */
    [[nodiscard]] bool contains(const entity_type entt) const noexcept {
        return *this && handle().contains(entt) && (handle().index(entt) < (descriptor->length()));
    }

    /**
     * @brief Returns the components assigned to the given entity.
     *
     * @warning
     * Attempting to use an entity that doesn't belong to the group results in
     * undefined behavior.
     *
     * @tparam Type Type of the component to get.
     * @tparam Other Other types of components to get.
     * @param entt A valid identifier.
     * @return The components assigned to the entity.
     */
    template<typename Type, typename... Other>
    [[nodiscard]] decltype(auto) get(const entity_type entt) const {
        return get<index_of<Type>, index_of<Other>...>(entt);
    }

    /**
     * @brief Returns the components assigned to the given entity.
     *
     * @warning
     * Attempting to use an entity that doesn't belong to the groups results in
     * undefined behavior.
     *
     * @tparam Index Indexes of the components to get.
     * @param entt A valid identifier.
     * @return The components assigned to the entity.
     */
    template<std::size_t... Index>
    [[nodiscard]] decltype(auto) get(const entity_type entt) const {
        const auto cpools = pools();

        if constexpr(sizeof...(Index) == 0) {
            return std::apply([entt](auto *...curr) { return std::tuple_cat(curr->get_as_tuple(entt)...); }, cpools);
        } else if constexpr(sizeof...(Index) == 1) {
            return (std::get<Index>(cpools)->get(entt), ...);
        } else {
            return std::tuple_cat(std::get<Index>(cpools)->get_as_tuple(entt)...);
        }
    }

    /**
     * @brief Iterates entities and components and applies the given function
     * object to them.
     *
     * The function object is invoked for each entity. It is provided with the
     * entity itself and a set of references to non-empty components. The
     * _constness_ of the components is as requested.<br/>
     * The signature of the function must be equivalent to one of the following
     * forms:
     *
     * @code{.cpp}
     * void(const entity_type, Type &...);
     * void(Type &...);
     * @endcode
     *
     * @note
     * Empty types aren't explicitly instantiated and therefore they are never
     * returned during iterations.
     *
     * @tparam Func Type of the function object to invoke.
     * @param func A valid function object.
     */
    template<typename Func>
    void each(Func func) const {
        for(auto args: each()) {
            if constexpr(is_applicable_v<Func, decltype(std::tuple_cat(std::tuple<entity_type>{}, std::declval<basic_group>().get({})))>) {
                std::apply(func, args);
            } else {
                std::apply([&func](auto, auto &&...less) { func(std::forward<decltype(less)>(less)...); }, args);
            }
        }
    }

    /**
     * @brief Returns an iterable object to use to _visit_ a group.
     *
     * The iterable object returns tuples that contain the current entity and a
     * set of references to its non-empty components. The _constness_ of the
     * components is as requested.
     *
     * @note
     * Empty types aren't explicitly instantiated and therefore they are never
     * returned during iterations.
     *
     * @return An iterable object to use to _visit_ the group.
     */
    [[nodiscard]] iterable each() const noexcept {
        const auto cpools = pools();
        return {{begin(), cpools}, {end(), cpools}};
    }

    /**
     * @brief Sort a group according to the given comparison function.
     *
     * The comparison function object must return `true` if the first element
     * is _less_ than the second one, `false` otherwise. The signature of the
     * comparison function should be equivalent to one of the following:
     *
     * @code{.cpp}
     * bool(std::tuple<Type &...>, std::tuple<Type &...>);
     * bool(const Type &, const Type &);
     * bool(const Entity, const Entity);
     * @endcode
     *
     * Where `Type` are either owned types or not but still such that they are
     * iterated by the group.<br/>
     * Moreover, the comparison function object shall induce a
     * _strict weak ordering_ on the values.
     *
     * The sort function object must offer a member function template
     * `operator()` that accepts three arguments:
     *
     * * An iterator to the first element of the range to sort.
     * * An iterator past the last element of the range to sort.
     * * A comparison function to use to compare the elements.
     *
     * @tparam Type Optional types of components to compare.
     * @tparam Compare Type of comparison function object.
     * @tparam Sort Type of sort function object.
     * @tparam Args Types of arguments to forward to the sort function object.
     * @param compare A valid comparison function object.
     * @param algo A valid sort function object.
     * @param args Arguments to forward to the sort function object, if any.
     */
    template<typename... Type, typename Compare, typename Sort = std_sort, typename... Args>
    void sort(Compare compare, Sort algo = Sort{}, Args &&...args) const {
        const auto cpools = pools();

        if constexpr(sizeof...(Type) == 0) {
            static_assert(std::is_invocable_v<Compare, const entity_type, const entity_type>, "Invalid comparison function");
            storage<0>().sort_n(descriptor->length(), std::move(compare), std::move(algo), std::forward<Args>(args)...);
        } else {
            auto comp = [&compare, &cpools](const entity_type lhs, const entity_type rhs) {
                if constexpr(sizeof...(Type) == 1) {
                    return compare((std::get<index_of<Type>>(cpools)->get(lhs), ...), (std::get<index_of<Type>>(cpools)->get(rhs), ...));
                } else {
                    return compare(std::forward_as_tuple(std::get<index_of<Type>>(cpools)->get(lhs)...), std::forward_as_tuple(std::get<index_of<Type>>(cpools)->get(rhs)...));
                }
            };

            storage<0>().sort_n(descriptor->length(), std::move(comp), std::move(algo), std::forward<Args>(args)...);
        }

        auto cb = [this](auto *head, auto *...other) {
            for(auto next = descriptor->length(); next; --next) {
                const auto pos = next - 1;
                [[maybe_unused]] const auto entt = head->data()[pos];
                (other->swap_elements(other->data()[pos], entt), ...);
            }
        };

        std::apply(cb, cpools);
    }

private:
    handler *descriptor;
};

} // namespace entt

#endif
