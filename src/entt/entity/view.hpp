#ifndef ENTT_ENTITY_VIEW_HPP
#define ENTT_ENTITY_VIEW_HPP

#include <algorithm>
#include <array>
#include <iterator>
#include <tuple>
#include <type_traits>
#include <utility>
#include "../config/config.h"
#include "../core/iterator.hpp"
#include "../core/type_traits.hpp"
#include "component.hpp"
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

template<typename Type, std::size_t Component, std::size_t Exclude>
class view_iterator final {
    using iterator_type = typename Type::const_iterator;

    [[nodiscard]] bool valid() const noexcept {
        return ((Component != 0u) || (*it != tombstone))
               && std::apply([entt = *it](const auto *...curr) { return (curr->contains(entt) && ...); }, pools)
               && std::apply([entt = *it](const auto *...curr) { return (!curr->contains(entt) && ...); }, filter);
    }

public:
    using value_type = typename iterator_type::value_type;
    using pointer = typename iterator_type::pointer;
    using reference = typename iterator_type::reference;
    using difference_type = typename iterator_type::difference_type;
    using iterator_category = std::forward_iterator_tag;

    constexpr view_iterator() noexcept
        : it{},
          last{},
          pools{},
          filter{} {}

    view_iterator(iterator_type curr, iterator_type to, std::array<const Type *, Component> all_of, std::array<const Type *, Exclude> none_of) noexcept
        : it{curr},
          last{to},
          pools{all_of},
          filter{none_of} {
        if(it != last && !valid()) {
            ++(*this);
        }
    }

    view_iterator &operator++() noexcept {
        while(++it != last && !valid()) {}
        return *this;
    }

    view_iterator operator++(int) noexcept {
        view_iterator orig = *this;
        return ++(*this), orig;
    }

    [[nodiscard]] pointer operator->() const noexcept {
        return &*it;
    }

    [[nodiscard]] reference operator*() const noexcept {
        return *operator->();
    }

    template<typename LhsType, auto... LhsArgs, typename RhsType, auto... RhsArgs>
    friend constexpr bool operator==(const view_iterator<LhsType, LhsArgs...> &, const view_iterator<RhsType, RhsArgs...> &) noexcept;

private:
    iterator_type it;
    iterator_type last;
    std::array<const Type *, Component> pools;
    std::array<const Type *, Exclude> filter;
};

template<typename LhsType, auto... LhsArgs, typename RhsType, auto... RhsArgs>
[[nodiscard]] constexpr bool operator==(const view_iterator<LhsType, LhsArgs...> &lhs, const view_iterator<RhsType, RhsArgs...> &rhs) noexcept {
    return lhs.it == rhs.it;
}

template<typename LhsType, auto... LhsArgs, typename RhsType, auto... RhsArgs>
[[nodiscard]] constexpr bool operator!=(const view_iterator<LhsType, LhsArgs...> &lhs, const view_iterator<RhsType, RhsArgs...> &rhs) noexcept {
    return !(lhs == rhs);
}

template<typename It, typename... Type>
struct extended_view_iterator final {
    using difference_type = std::ptrdiff_t;
    using value_type = decltype(std::tuple_cat(std::make_tuple(*std::declval<It>()), std::declval<Type>().get_as_tuple({})...));
    using pointer = input_iterator_pointer<value_type>;
    using reference = value_type;
    using iterator_category = std::input_iterator_tag;

    constexpr extended_view_iterator()
        : it{},
          pools{} {}

    extended_view_iterator(It from, std::tuple<Type *...> storage)
        : it{from},
          pools{storage} {}

    extended_view_iterator &operator++() noexcept {
        return ++it, *this;
    }

    extended_view_iterator operator++(int) noexcept {
        extended_view_iterator orig = *this;
        return ++(*this), orig;
    }

    [[nodiscard]] reference operator*() const noexcept {
        return std::apply([entt = *it](auto *...curr) { return std::tuple_cat(std::make_tuple(entt), curr->get_as_tuple(entt)...); }, pools);
    }

    [[nodiscard]] pointer operator->() const noexcept {
        return operator*();
    }

    template<typename... Lhs, typename... Rhs>
    friend bool constexpr operator==(const extended_view_iterator<Lhs...> &, const extended_view_iterator<Rhs...> &) noexcept;

private:
    It it;
    std::tuple<Type *...> pools;
};

template<typename... Lhs, typename... Rhs>
[[nodiscard]] constexpr bool operator==(const extended_view_iterator<Lhs...> &lhs, const extended_view_iterator<Rhs...> &rhs) noexcept {
    return lhs.it == rhs.it;
}

template<typename... Lhs, typename... Rhs>
[[nodiscard]] constexpr bool operator!=(const extended_view_iterator<Lhs...> &lhs, const extended_view_iterator<Rhs...> &rhs) noexcept {
    return !(lhs == rhs);
}

} // namespace internal

/**
 * Internal details not to be documented.
 * @endcond
 */

/**
 * @brief View implementation.
 *
 * Primary template isn't defined on purpose. All the specializations give a
 * compile-time error, but for a few reasonable cases.
 */
template<typename, typename, typename, typename>
class basic_view;

/**
 * @brief Multi component view.
 *
 * Multi component views iterate over those entities that have at least all the
 * given components in their bags. During initialization, a multi component view
 * looks at the number of entities available for each component and uses the
 * smallest set in order to get a performance boost when iterate.
 *
 * @b Important
 *
 * Iterators aren't invalidated if:
 *
 * * New instances of the given components are created and assigned to entities.
 * * The entity currently pointed is modified (as an example, if one of the
 *   given components is removed from the entity to which the iterator points).
 * * The entity currently pointed is destroyed.
 *
 * In all other cases, modifying the pools iterated by the view in any way
 * invalidates all the iterators and using them results in undefined behavior.
 *
 * @tparam Entity A valid entity type (see entt_traits for more details).
 * @tparam Component Types of components iterated by the view.
 * @tparam Exclude Types of components used to filter the view.
 */
template<typename Entity, typename... Component, typename... Exclude>
class basic_view<Entity, get_t<Component...>, exclude_t<Exclude...>> {
    template<typename, typename, typename, typename>
    friend class basic_view;

    template<typename Comp>
    using storage_type = constness_as_t<typename storage_traits<Entity, std::remove_const_t<Comp>>::type, Comp>;

    [[nodiscard]] auto opaque_check() const noexcept {
        std::array<const base_type *, sizeof...(Component) - 1u> other{};
        std::apply([&other, pos = 0u, view = view](const auto *...curr) mutable { ((curr == view ? void() : void(other[pos++] = curr)), ...); }, pools);
        return other;
    }

    [[nodiscard]] auto opaque_filter() const noexcept {
        return std::apply([](const auto *...curr) { return std::array<const base_type *, sizeof...(Exclude)>{curr...}; }, filter);
    }

    template<std::size_t Comp, std::size_t Other, typename... Args>
    [[nodiscard]] auto dispatch_get(const std::tuple<Entity, Args...> &curr) const {
        if constexpr(Comp == Other) {
            return std::forward_as_tuple(std::get<Args>(curr)...);
        } else {
            return std::get<Other>(pools)->get_as_tuple(std::get<0>(curr));
        }
    }

    template<std::size_t Comp, typename Func, std::size_t... Index>
    void each(Func func, std::index_sequence<Index...>) const {
        for(const auto curr: std::get<Comp>(pools)->each()) {
            const auto entt = std::get<0>(curr);

            if(((sizeof...(Component) != 1u) || (entt != tombstone))
               && ((Comp == Index || std::get<Index>(pools)->contains(entt)) && ...)
               && std::apply([entt](const auto *...epool) { return (!epool->contains(entt) && ...); }, filter)) {
                if constexpr(is_applicable_v<Func, decltype(std::tuple_cat(std::tuple<entity_type>{}, std::declval<basic_view>().get({})))>) {
                    std::apply(func, std::tuple_cat(std::make_tuple(entt), dispatch_get<Comp, Index>(curr)...));
                } else {
                    std::apply(func, std::tuple_cat(dispatch_get<Comp, Index>(curr)...));
                }
            }
        }
    }

    template<typename Func, std::size_t... Index>
    void pick_and_each(Func func, std::index_sequence<Index...> seq) const {
        ((std::get<Index>(pools) == view ? each<Index>(std::move(func), seq) : void()), ...);
    }

public:
    /*! @brief Underlying entity identifier. */
    using entity_type = Entity;
    /*! @brief Unsigned integer type. */
    using size_type = std::size_t;
    /*! @brief Common type among all storage types. */
    using base_type = std::common_type_t<typename storage_type<Component>::base_type...>;
    /*! @brief Bidirectional iterator type. */
    using iterator = internal::view_iterator<base_type, sizeof...(Component) - 1u, sizeof...(Exclude)>;
    /*! @brief Iterable view type. */
    using iterable = iterable_adaptor<internal::extended_view_iterator<iterator, storage_type<Component>...>>;

    /*! @brief Default constructor to use to create empty, invalid views. */
    basic_view() noexcept
        : pools{},
          filter{},
          view{} {}

    /**
     * @brief Constructs a multi-type view from a set of storage classes.
     * @param component The storage for the types to iterate.
     * @param epool The storage for the types used to filter the view.
     */
    basic_view(storage_type<Component> &...component, storage_type<Exclude> &...epool) noexcept
        : basic_view{std::forward_as_tuple(component...), std::forward_as_tuple(epool...)} {}

    /**
     * @brief Constructs a multi-type view from a set of storage classes.
     * @param component The storage for the types to iterate.
     * @param epool The storage for the types used to filter the view.
     */
    basic_view(std::tuple<storage_type<Component> &...> component, std::tuple<storage_type<Exclude> &...> epool = {}) noexcept
        : pools{std::apply([](auto &...curr) { return std::make_tuple(&curr...); }, component)},
          filter{std::apply([](auto &...curr) { return std::make_tuple(&curr...); }, epool)},
          view{std::apply([](const auto &...curr) { return (std::min)({&static_cast<const base_type &>(curr)...}, [](auto *lhs, auto *rhs) { return lhs->size() < rhs->size(); }); }, component)} {}

    /**
     * @brief Creates a new view driven by a given component in its iterations.
     * @tparam Comp Type of component used to drive the iteration.
     * @return A new view driven by the given component in its iterations.
     */
    template<typename Comp>
    [[nodiscard]] basic_view use() const noexcept {
        basic_view other{*this};
        other.view = std::get<storage_type<Comp> *>(pools);
        return other;
    }

    /**
     * @brief Creates a new view driven by a given component in its iterations.
     * @tparam Comp Index of the component used to drive the iteration.
     * @return A new view driven by the given component in its iterations.
     */
    template<std::size_t Comp>
    [[nodiscard]] basic_view use() const noexcept {
        basic_view other{*this};
        other.view = std::get<Comp>(pools);
        return other;
    }

    /**
     * @brief Returns the leading storage of a view.
     * @return The leading storage of the view.
     */
    const base_type &handle() const noexcept {
        return *view;
    }

    /**
     * @brief Returns the storage for a given component type.
     * @tparam Comp Type of component of which to return the storage.
     * @return The storage for the given component type.
     */
    template<typename Comp>
    [[nodiscard]] decltype(auto) storage() const noexcept {
        return *std::get<storage_type<Comp> *>(pools);
    }

    /**
     * @brief Returns the storage for a given component type.
     * @tparam Comp Index of component of which to return the storage.
     * @return The storage for the given component type.
     */
    template<std::size_t Comp>
    [[nodiscard]] decltype(auto) storage() const noexcept {
        return *std::get<Comp>(pools);
    }

    /**
     * @brief Estimates the number of entities iterated by the view.
     * @return Estimated number of entities iterated by the view.
     */
    [[nodiscard]] size_type size_hint() const noexcept {
        return view->size();
    }

    /**
     * @brief Returns an iterator to the first entity of the view.
     *
     * The returned iterator points to the first entity of the view. If the view
     * is empty, the returned iterator will be equal to `end()`.
     *
     * @return An iterator to the first entity of the view.
     */
    [[nodiscard]] iterator begin() const noexcept {
        return iterator{view->begin(), view->end(), opaque_check(), opaque_filter()};
    }

    /**
     * @brief Returns an iterator that is past the last entity of the view.
     *
     * The returned iterator points to the entity following the last entity of
     * the view. Attempting to dereference the returned iterator results in
     * undefined behavior.
     *
     * @return An iterator to the entity following the last entity of the view.
     */
    [[nodiscard]] iterator end() const noexcept {
        return iterator{view->end(), view->end(), opaque_check(), opaque_filter()};
    }

    /**
     * @brief Returns the first entity of the view, if any.
     * @return The first entity of the view if one exists, the null entity
     * otherwise.
     */
    [[nodiscard]] entity_type front() const noexcept {
        const auto it = begin();
        return it != end() ? *it : null;
    }

    /**
     * @brief Returns the last entity of the view, if any.
     * @return The last entity of the view if one exists, the null entity
     * otherwise.
     */
    [[nodiscard]] entity_type back() const noexcept {
        auto it = view->rbegin();
        for(const auto last = view->rend(); it != last && !contains(*it); ++it) {}
        return it == view->rend() ? null : *it;
    }

    /**
     * @brief Finds an entity.
     * @param entt A valid identifier.
     * @return An iterator to the given entity if it's found, past the end
     * iterator otherwise.
     */
    [[nodiscard]] iterator find(const entity_type entt) const noexcept {
        return contains(entt) ? iterator{view->find(entt), view->end(), opaque_check(), opaque_filter()} : end();
    }

    /**
     * @brief Returns the components assigned to the given entity.
     * @param entt A valid identifier.
     * @return The components assigned to the given entity.
     */
    [[nodiscard]] decltype(auto) operator[](const entity_type entt) const {
        return get<Component...>(entt);
    }

    /**
     * @brief Checks if a view is properly initialized.
     * @return True if the view is properly initialized, false otherwise.
     */
    [[nodiscard]] explicit operator bool() const noexcept {
        return view != nullptr;
    }

    /**
     * @brief Checks if a view contains an entity.
     * @param entt A valid identifier.
     * @return True if the view contains the given entity, false otherwise.
     */
    [[nodiscard]] bool contains(const entity_type entt) const noexcept {
        return std::apply([entt](const auto *...curr) { return (curr->contains(entt) && ...); }, pools)
               && std::apply([entt](const auto *...curr) { return (!curr->contains(entt) && ...); }, filter);
    }

    /**
     * @brief Returns the components assigned to the given entity.
     *
     * @warning
     * Attempting to use an entity that doesn't belong to the view results in
     * undefined behavior.
     *
     * @tparam Comp Types of components to get.
     * @param entt A valid identifier.
     * @return The components assigned to the entity.
     */
    template<typename... Comp>
    [[nodiscard]] decltype(auto) get(const entity_type entt) const {
        ENTT_ASSERT(contains(entt), "View does not contain entity");

        if constexpr(sizeof...(Comp) == 0) {
            return std::apply([entt](auto *...curr) { return std::tuple_cat(curr->get_as_tuple(entt)...); }, pools);
        } else if constexpr(sizeof...(Comp) == 1) {
            return (std::get<storage_type<Comp> *>(pools)->get(entt), ...);
        } else {
            return std::tuple_cat(std::get<storage_type<Comp> *>(pools)->get_as_tuple(entt)...);
        }
    }

    /**
     * @brief Returns the components assigned to the given entity.
     *
     * @warning
     * Attempting to use an entity that doesn't belong to the view results in
     * undefined behavior.
     *
     * @tparam First Index of a component to get.
     * @tparam Other Indexes of other components to get.
     * @param entt A valid identifier.
     * @return The components assigned to the entity.
     */
    template<std::size_t First, std::size_t... Other>
    [[nodiscard]] decltype(auto) get(const entity_type entt) const {
        ENTT_ASSERT(contains(entt), "View does not contain entity");

        if constexpr(sizeof...(Other) == 0) {
            return std::get<First>(pools)->get(entt);
        } else {
            return std::tuple_cat(std::get<First>(pools)->get_as_tuple(entt), std::get<Other>(pools)->get_as_tuple(entt)...);
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
     * @tparam Func Type of the function object to invoke.
     * @param func A valid function object.
     */
    template<typename Func>
    void each(Func func) const {
        pick_and_each(std::move(func), std::index_sequence_for<Component...>{});
    }

    /**
     * @brief Returns an iterable object to use to _visit_ a view.
     *
     * The iterable object returns a tuple that contains the current entity and
     * a set of references to its non-empty components. The _constness_ of the
     * components is as requested.
     *
     * @return An iterable object to use to _visit_ the view.
     */
    [[nodiscard]] iterable each() const noexcept {
        return {internal::extended_view_iterator{begin(), pools}, internal::extended_view_iterator{end(), pools}};
    }

    /**
     * @brief Combines two views in a _more specific_ one (friend function).
     * @tparam Get Component list of the view to combine with.
     * @tparam Excl Filter list of the view to combine with.
     * @param other The view to combine with.
     * @return A more specific view.
     */
    template<typename... Get, typename... Excl>
    [[nodiscard]] auto operator|(const basic_view<Entity, get_t<Get...>, exclude_t<Excl...>> &other) const noexcept {
        using view_type = basic_view<Entity, get_t<Component..., Get...>, exclude_t<Exclude..., Excl...>>;
        return std::make_from_tuple<view_type>(std::apply([](auto *...curr) { return std::forward_as_tuple(*curr...); }, std::tuple_cat(pools, other.pools, filter, other.filter)));
    }

private:
    std::tuple<storage_type<Component> *...> pools;
    std::tuple<storage_type<Exclude> *...> filter;
    const base_type *view;
};

/**
 * @brief Single component view specialization.
 *
 * Single component views are specialized in order to get a boost in terms of
 * performance. This kind of views can access the underlying data structure
 * directly and avoid superfluous checks.
 *
 * @b Important
 *
 * Iterators aren't invalidated if:
 *
 * * New instances of the given component are created and assigned to entities.
 * * The entity currently pointed is modified (as an example, the given
 *   component is removed from the entity to which the iterator points).
 * * The entity currently pointed is destroyed.
 *
 * In all other cases, modifying the pool iterated by the view in any way
 * invalidates all the iterators and using them results in undefined behavior.
 *
 * @tparam Entity A valid entity type (see entt_traits for more details).
 * @tparam Component Type of component iterated by the view.
 */
template<typename Entity, typename Component>
class basic_view<Entity, get_t<Component>, exclude_t<>, std::void_t<std::enable_if_t<!component_traits<std::remove_const_t<Component>>::in_place_delete>>> {
    template<typename, typename, typename, typename>
    friend class basic_view;

    using storage_type = constness_as_t<typename storage_traits<Entity, std::remove_const_t<Component>>::type, Component>;

public:
    /*! @brief Underlying entity identifier. */
    using entity_type = Entity;
    /*! @brief Unsigned integer type. */
    using size_type = std::size_t;
    /*! @brief Common type among all storage types. */
    using base_type = typename storage_type::base_type;
    /*! @brief Random access iterator type. */
    using iterator = typename base_type::iterator;
    /*! @brief Reversed iterator type. */
    using reverse_iterator = typename base_type::reverse_iterator;
    /*! @brief Iterable view type. */
    using iterable = decltype(std::declval<storage_type>().each());

    /*! @brief Default constructor to use to create empty, invalid views. */
    basic_view() noexcept
        : pools{},
          filter{},
          view{} {}

    /**
     * @brief Constructs a single-type view from a storage class.
     * @param ref The storage for the type to iterate.
     */
    basic_view(storage_type &ref) noexcept
        : basic_view{std::forward_as_tuple(ref)} {}

    /**
     * @brief Constructs a single-type view from a storage class.
     * @param ref The storage for the type to iterate.
     */
    basic_view(std::tuple<storage_type &> ref, std::tuple<> = {}) noexcept
        : pools{&std::get<0>(ref)},
          filter{},
          view{&std::get<0>(ref)} {}

    /**
     * @brief Returns the leading storage of a view.
     * @return The leading storage of the view.
     */
    const base_type &handle() const noexcept {
        return *view;
    }

    /**
     * @brief Returns the storage for a given component type.
     * @tparam Comp Type of component of which to return the storage.
     * @return The storage for the given component type.
     */
    template<typename Comp = Component>
    [[nodiscard]] decltype(auto) storage() const noexcept {
        static_assert(std::is_same_v<Comp, Component>, "Invalid component type");
        return *std::get<0>(pools);
    }

    /**
     * @brief Returns the storage for a given component type.
     * @tparam Comp Index of component of which to return the storage.
     * @return The storage for the given component type.
     */
    template<std::size_t Comp>
    [[nodiscard]] decltype(auto) storage() const noexcept {
        return *std::get<Comp>(pools);
    }

    /**
     * @brief Returns the number of entities that have the given component.
     * @return Number of entities that have the given component.
     */
    [[nodiscard]] size_type size() const noexcept {
        return view->size();
    }

    /**
     * @brief Checks whether a view is empty.
     * @return True if the view is empty, false otherwise.
     */
    [[nodiscard]] bool empty() const noexcept {
        return view->empty();
    }

    /**
     * @brief Returns an iterator to the first entity of the view.
     *
     * The returned iterator points to the first entity of the view. If the view
     * is empty, the returned iterator will be equal to `end()`.
     *
     * @return An iterator to the first entity of the view.
     */
    [[nodiscard]] iterator begin() const noexcept {
        return view->begin();
    }

    /**
     * @brief Returns an iterator that is past the last entity of the view.
     *
     * The returned iterator points to the entity following the last entity of
     * the view. Attempting to dereference the returned iterator results in
     * undefined behavior.
     *
     * @return An iterator to the entity following the last entity of the view.
     */
    [[nodiscard]] iterator end() const noexcept {
        return view->end();
    }

    /**
     * @brief Returns an iterator to the first entity of the reversed view.
     *
     * The returned iterator points to the first entity of the reversed view. If
     * the view is empty, the returned iterator will be equal to `rend()`.
     *
     * @return An iterator to the first entity of the reversed view.
     */
    [[nodiscard]] reverse_iterator rbegin() const noexcept {
        return view->rbegin();
    }

    /**
     * @brief Returns an iterator that is past the last entity of the reversed
     * view.
     *
     * The returned iterator points to the entity following the last entity of
     * the reversed view. Attempting to dereference the returned iterator
     * results in undefined behavior.
     *
     * @return An iterator to the entity following the last entity of the
     * reversed view.
     */
    [[nodiscard]] reverse_iterator rend() const noexcept {
        return view->rend();
    }

    /**
     * @brief Returns the first entity of the view, if any.
     * @return The first entity of the view if one exists, the null entity
     * otherwise.
     */
    [[nodiscard]] entity_type front() const noexcept {
        return empty() ? null : *begin();
    }

    /**
     * @brief Returns the last entity of the view, if any.
     * @return The last entity of the view if one exists, the null entity
     * otherwise.
     */
    [[nodiscard]] entity_type back() const noexcept {
        return empty() ? null : *rbegin();
    }

    /**
     * @brief Finds an entity.
     * @param entt A valid identifier.
     * @return An iterator to the given entity if it's found, past the end
     * iterator otherwise.
     */
    [[nodiscard]] iterator find(const entity_type entt) const noexcept {
        return contains(entt) ? view->find(entt) : end();
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
     * @brief Returns the component assigned to the given entity.
     * @param entt A valid identifier.
     * @return The component assigned to the given entity.
     */
    [[nodiscard]] decltype(auto) operator[](const entity_type entt) const {
        return get<Component>(entt);
    }

    /**
     * @brief Checks if a view is properly initialized.
     * @return True if the view is properly initialized, false otherwise.
     */
    [[nodiscard]] explicit operator bool() const noexcept {
        return view != nullptr;
    }

    /**
     * @brief Checks if a view contains an entity.
     * @param entt A valid identifier.
     * @return True if the view contains the given entity, false otherwise.
     */
    [[nodiscard]] bool contains(const entity_type entt) const noexcept {
        return view->contains(entt);
    }

    /**
     * @brief Returns the component assigned to the given entity.
     *
     * @warning
     * Attempting to use an entity that doesn't belong to the view results in
     * undefined behavior.
     *
     * @tparam Comp Type or index of the component to get.
     * @param entt A valid identifier.
     * @return The component assigned to the entity.
     */
    template<typename... Comp>
    [[nodiscard]] decltype(auto) get(const entity_type entt) const {
        ENTT_ASSERT(contains(entt), "View does not contain entity");

        if constexpr(sizeof...(Comp) == 0) {
            return std::get<0>(pools)->get_as_tuple(entt);
        } else {
            static_assert(std::is_same_v<Comp..., Component>, "Invalid component type");
            return std::get<0>(pools)->get(entt);
        }
    }

    /*! @copydoc get */
    template<std::size_t Comp>
    [[nodiscard]] decltype(auto) get(const entity_type entt) const {
        ENTT_ASSERT(contains(entt), "View does not contain entity");
        return std::get<0>(pools)->get(entt);
    }

    /**
     * @brief Iterates entities and components and applies the given function
     * object to them.
     *
     * The function object is invoked for each entity. It is provided with the
     * entity itself and a reference to the component if it's a non-empty one.
     * The _constness_ of the component is as requested.<br/>
     * The signature of the function must be equivalent to one of the following
     * forms:
     *
     * @code{.cpp}
     * void(const entity_type, Component &);
     * void(Component &);
     * @endcode
     *
     * @tparam Func Type of the function object to invoke.
     * @param func A valid function object.
     */
    template<typename Func>
    void each(Func func) const {
        if constexpr(is_applicable_v<Func, decltype(*each().begin())>) {
            for(const auto pack: each()) {
                std::apply(func, pack);
            }
        } else if constexpr(std::is_invocable_v<Func, Component &>) {
            for(auto &&component: *std::get<0>(pools)) {
                func(component);
            }
        } else if constexpr(std::is_invocable_v<Func, Entity>) {
            for(auto entity: *view) {
                func(entity);
            }
        } else {
            for(size_type pos{}, last = size(); pos < last; ++pos) {
                func();
            }
        }
    }

    /**
     * @brief Returns an iterable object to use to _visit_ a view.
     *
     * The iterable object returns a tuple that contains the current entity and
     * a reference to its component if it's a non-empty one. The _constness_ of
     * the component is as requested.
     *
     * @return An iterable object to use to _visit_ the view.
     */
    [[nodiscard]] iterable each() const noexcept {
        return std::get<0>(pools)->each();
    }

    /**
     * @brief Combines two views in a _more specific_ one (friend function).
     * @tparam Get Component list of the view to combine with.
     * @tparam Excl Filter list of the view to combine with.
     * @param other The view to combine with.
     * @return A more specific view.
     */
    template<typename... Get, typename... Excl>
    [[nodiscard]] auto operator|(const basic_view<Entity, get_t<Get...>, exclude_t<Excl...>> &other) const noexcept {
        using view_type = basic_view<Entity, get_t<Component, Get...>, exclude_t<Excl...>>;
        return std::make_from_tuple<view_type>(std::apply([this](auto *...curr) { return std::forward_as_tuple(*std::get<0>(pools), *curr...); }, std::tuple_cat(other.pools, other.filter)));
    }

private:
    std::tuple<storage_type *> pools;
    std::tuple<> filter;
    const base_type *view;
};

/**
 * @brief Deduction guide.
 * @tparam Type Type of storage classes used to create the view.
 * @param storage The storage for the types to iterate.
 */
template<typename... Type>
basic_view(Type &...storage) -> basic_view<std::common_type_t<typename Type::entity_type...>, get_t<constness_as_t<typename Type::value_type, Type>...>, exclude_t<>>;

/**
 * @brief Deduction guide.
 * @tparam Component Types of components iterated by the view.
 * @tparam Exclude Types of components used to filter the view.
 */
template<typename... Component, typename... Exclude>
basic_view(std::tuple<Component &...>, std::tuple<Exclude &...> = {}) -> basic_view<std::common_type_t<typename Component::entity_type..., typename Exclude::entity_type...>, get_t<constness_as_t<typename Component::value_type, Component>...>, exclude_t<constness_as_t<typename Exclude::value_type, Exclude>...>>;

} // namespace entt

#endif
