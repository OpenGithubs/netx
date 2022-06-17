#ifndef ENTT_GRAPH_ADJACENCY_MATRIX_HPP
#define ENTT_GRAPH_ADJACENCY_MATRIX_HPP

#include <cstddef>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>
#include "../config/config.h"
#include "../core/iterator.hpp"
#include "fwd.hpp"

namespace entt {

/**
 * @cond TURN_OFF_DOXYGEN
 * Internal details not to be documented.
 */

namespace internal {

template<typename It>
class row_edge_iterator {
    using size_type = std::size_t;

public:
    using value_type = std::pair<size_type, size_type>;
    using pointer = input_iterator_pointer<value_type>;
    using reference = value_type;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::input_iterator_tag;

    constexpr row_edge_iterator() noexcept
        : it{},
          vert{},
          pos{},
          last{} {}

    constexpr row_edge_iterator(It base, const size_type vertices, const size_type from, const size_type to) noexcept
        : it{std::move(base)},
          vert{vertices},
          pos{from},
          last{to} {
        for(; pos != last && !it[pos]; ++pos) {}
    }

    constexpr row_edge_iterator &operator++() noexcept {
        for(++pos; pos != last && !it[pos]; ++pos) {}
        return *this;
    }

    constexpr row_edge_iterator operator++(int) noexcept {
        row_edge_iterator orig = *this;
        return ++(*this), orig;
    }

    [[nodiscard]] constexpr reference operator*() const noexcept {
        return *operator->();
    }

    [[nodiscard]] constexpr pointer operator->() const noexcept {
        return std::make_pair<size_type>(pos / vert, pos % vert);
    }

    template<typename Type>
    friend constexpr bool operator==(const row_edge_iterator<Type> &, const row_edge_iterator<Type> &) noexcept;

private:
    It it;
    size_type vert;
    size_type pos;
    size_type last;
};

template<typename Container>
[[nodiscard]] inline constexpr bool operator==(const row_edge_iterator<Container> &lhs, const row_edge_iterator<Container> &rhs) noexcept {
    return lhs.pos == rhs.pos;
}

template<typename Container>
[[nodiscard]] inline constexpr bool operator!=(const row_edge_iterator<Container> &lhs, const row_edge_iterator<Container> &rhs) noexcept {
    return !(lhs == rhs);
}

template<typename It>
class col_edge_iterator {
    using size_type = std::size_t;

public:
    using value_type = std::pair<size_type, size_type>;
    using pointer = input_iterator_pointer<value_type>;
    using reference = value_type;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::input_iterator_tag;

    constexpr col_edge_iterator() noexcept
        : it{},
          vert{},
          pos{},
          last{} {}

    constexpr col_edge_iterator(It base, const size_type vertices, const size_type from, const size_type to) noexcept
        : it{std::move(base)},
          vert{vertices},
          pos{from},
          last{to} {
        for(; pos != last && !it[pos]; pos += vert) {}
    }

    constexpr col_edge_iterator &operator++() noexcept {
        for(pos += vert; pos != last && !it[pos]; pos += vert) {}
        return *this;
    }

    constexpr col_edge_iterator operator++(int) noexcept {
        col_edge_iterator orig = *this;
        return ++(*this), orig;
    }

    [[nodiscard]] constexpr reference operator*() const noexcept {
        return *operator->();
    }

    [[nodiscard]] constexpr pointer operator->() const noexcept {
        return std::make_pair<size_type>(pos / vert, pos % vert);
    }

    template<typename Type>
    friend constexpr bool operator==(const col_edge_iterator<Type> &, const col_edge_iterator<Type> &) noexcept;

private:
    It it;
    size_type vert;
    size_type pos;
    size_type last;
};

template<typename Container>
[[nodiscard]] inline constexpr bool operator==(const col_edge_iterator<Container> &lhs, const col_edge_iterator<Container> &rhs) noexcept {
    return lhs.pos == rhs.pos;
}

template<typename Container>
[[nodiscard]] inline constexpr bool operator!=(const col_edge_iterator<Container> &lhs, const col_edge_iterator<Container> &rhs) noexcept {
    return !(lhs == rhs);
}

} // namespace internal

/**
 * Internal details not to be documented.
 * @endcond
 */

/**
 * @brief Basic implementation of a directed adjacency matrix.
 * @tparam Allocator Type of allocator used to manage memory and elements.
 */
template<typename Allocator>
class basic_adjacency_matrix {
    using alloc_traits = std::allocator_traits<Allocator>;
    static_assert(std::is_same_v<typename alloc_traits::value_type, std::size_t>, "Invalid value type");
    using container_type = std::vector<std::size_t, typename alloc_traits::template rebind_alloc<std::size_t>>;

public:
    /*! @brief Allocator type. */
    using allocator_type = Allocator;
    /*! @brief Unsigned integer type. */
    using size_type = std::size_t;
    /*! @brief Vertex type. */
    using vertex_type = size_type;
    /*! @brief Edge type. */
    using edge_type = std::pair<vertex_type, vertex_type>;
    /*! @brief Vertex iterator type. */
    using vertex_iterator = iota_iterator<vertex_type>;
    /*! @brief Edge iterator type. */
    using edge_iterator = internal::row_edge_iterator<typename container_type::const_iterator>;
    /*! @brief Out edge iterator type. */
    using out_edge_iterator = internal::row_edge_iterator<typename container_type::const_iterator>;
    /*! @brief In edge iterator type. */
    using in_edge_iterator = internal::col_edge_iterator<typename container_type::const_iterator>;

    /*! @brief Default constructor. */
    basic_adjacency_matrix() noexcept(noexcept(allocator_type{}))
        : basic_adjacency_matrix{0u} {}

    /**
     * @brief Constructs an empty container with a given allocator.
     * @param allocator The allocator to use.
     */
    explicit basic_adjacency_matrix(const allocator_type &allocator) noexcept
        : basic_adjacency_matrix{0u, allocator} {}

    /**
     * @brief Constructs an empty container with a given allocator and user
     * supplied number of vertices.
     * @param vertices Number of vertices.
     * @param allocator The allocator to use.
     */
    basic_adjacency_matrix(const size_type vertices, const allocator_type &allocator = allocator_type{})
        : matrix(vertices * vertices),
          vert{vertices} {}

    /**
     * @brief Copy constructor.
     * @param other The instance to copy from.
     */
    basic_adjacency_matrix(const basic_adjacency_matrix &other)
        : basic_adjacency_matrix{other, other.get_allocator()} {}

    /**
     * @brief Allocator-extended copy constructor.
     * @param other The instance to copy from.
     * @param allocator The allocator to use.
     */
    basic_adjacency_matrix(const basic_adjacency_matrix &other, const allocator_type &allocator)
        : matrix{other.matrix, allocator},
          vert{other.vert} {}

    /**
     * @brief Move constructor.
     * @param other The instance to move from.
     */
    basic_adjacency_matrix(basic_adjacency_matrix &&other) noexcept
        : basic_adjacency_matrix{std::move(other), other.get_allocator()} {}

    /**
     * @brief Allocator-extended move constructor.
     * @param other The instance to move from.
     * @param allocator The allocator to use.
     */
    basic_adjacency_matrix(basic_adjacency_matrix &&other, const allocator_type &allocator)
        : matrix{std::move(other.matrix), allocator},
          vert{std::exchange(other.vert, 0u)} {}

    /**
     * @brief Default copy assignment operator.
     * @param other The instance to copy from.
     * @return This container.
     */
    basic_adjacency_matrix &operator=(const basic_adjacency_matrix &other) {
        matrix = other.matrix;
        vert = other.vert;
        return *this;
    }

    /**
     * @brief Default move assignment operator.
     * @param other The instance to move from.
     * @return This container.
     */
    basic_adjacency_matrix &operator=(basic_adjacency_matrix &&other) noexcept {
        matrix = std::move(other.matrix);
        vert = std::exchange(other.vert, 0u);
        return *this;
    }

    /**
     * @brief Returns the associated allocator.
     * @return The associated allocator.
     */
    [[nodiscard]] constexpr allocator_type get_allocator() const noexcept {
        return matrix.get_allocator();
    }

    /*! @brief Clears the adjacency matrix. */
    void clear() noexcept {
        matrix.clear();
        vert = {};
    }

    /**
     * @brief Exchanges the contents with those of a given adjacency matrix.
     * @param other Adjacency matrix to exchange the content with.
     */
    void swap(adjacency_matrix &other) {
        using std::swap;
        swap(matrix, other.matrix);
        swap(vert, other.vert);
    }

    /**
     * @brief Returns the number of vertices.
     * @return The number of vertices.
     */
    [[nodiscard]] size_type size() const noexcept {
        return vert;
    }

    /**
     * @brief Returns an iterable object to visit all vertices of a matrix.
     * @return An iterable object to visit all vertices of a matrix.
     */
    [[nodiscard]] iterable_adaptor<vertex_iterator> vertices() const noexcept {
        return {0u, vert};
    }

    /**
     * @brief Returns an iterable object to visit all edges of a matrix.
     * @return An iterable object to visit all edges of a matrix.
     */
    [[nodiscard]] iterable_adaptor<edge_iterator> edges() const noexcept {
        const auto it = matrix.cbegin();
        const auto sz = matrix.size();
        return {{it, vert, 0u, sz}, {it, vert, sz, sz}};
    }

    /**
     * @brief Returns an iterable object to visit all out edges of a vertex.
     * @param vertex The vertex of which to return all out edges.
     * @return An iterable object to visit all out edges of a vertex.
     */
    [[nodiscard]] iterable_adaptor<out_edge_iterator> out_edges(const vertex_type vertex) const noexcept {
        const auto it = matrix.cbegin();
        const auto from = vertex * vert;
        const auto to = vertex * vert + vert;
        return {{it, vert, from, to}, {it, vert, to, to}};
    }

    /**
     * @brief Returns an iterable object to visit all in edges of a vertex.
     * @param vertex The vertex of which to return all in edges.
     * @return An iterable object to visit all in edges of a vertex.
     */
    [[nodiscard]] iterable_adaptor<in_edge_iterator> in_edges(const vertex_type vertex) const noexcept {
        const auto it = matrix.cbegin();
        const auto from = vertex;
        const auto to = vertex * vert + vertex;
        return {{it, vert, from, to}, {it, vert, to, to}};
    }

    /**
     * @brief Resizes an adjacency matrix.
     * @param vertices The new number of vertices.
     */
    void resize(const size_type vertices) {
        matrix.resize(vertices * vertices);
        vert = vertices;
    }

    /**
     * @brief Inserts an edge into the adjacency matrix, if it does not exist.
     * @param lhs The left hand vertex of the edge.
     * @param rhs The right hand vertex of the edge.
     * @return A pair consisting of an iterator to the inserted element (or to
     * the element that prevented the insertion) and a bool denoting whether the
     * insertion took place.
     */
    std::pair<edge_iterator, bool> insert(const vertex_type lhs, const vertex_type rhs) {
        const auto pos = lhs * vert + rhs;
        const auto inserted = !std::exchange(matrix[pos], 1u);
        return {edge_iterator{matrix.cbegin(), vert, pos, matrix.size()}, inserted};
    }

    /**
     * @brief Removes the edge associated with a pair of given vertices.
     * @param lhs The left hand vertex of the edge.
     * @param rhs The right hand vertex of the edge.
     * @return Number of elements removed (either 0 or 1).
     */
    size_type erase(const vertex_type lhs, const vertex_type rhs) {
        return std::exchange(matrix[lhs * vert + rhs], 0u);
    }

    /**
     * @brief Checks if an adjacency matrix contains a given edge.
     * @param lhs The left hand vertex of the edge.
     * @param rhs The right hand vertex of the edge.
     * @return True if there is such an edge, false otherwise.
     */
    [[nodiscard]] bool contains(const vertex_type lhs, const vertex_type rhs) const {
        const auto pos = lhs * vert + rhs;
        return pos < matrix.size() && matrix[pos];
    }

private:
    container_type matrix;
    size_type vert;
};

} // namespace entt

#endif
