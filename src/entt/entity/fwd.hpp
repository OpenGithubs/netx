#ifndef ENTT_ENTITY_FWD_HPP
#define ENTT_ENTITY_FWD_HPP


#include "../core/fwd.hpp"


namespace entt {


/*! @class basic_registry */
template <typename>
class basic_registry;

/*! @class basic_view */
template<typename...>
class basic_view;

/*! @class basic_runtime_view */
template<typename>
class basic_runtime_view;

/*! @class basic_group */
template<typename...>
class basic_group;

/*! @class basic_observer */
template<typename>
class basic_observer;

/*! @struct basic_actor */
template <typename>
struct basic_actor;

/*! @class basic_snapshot */
template<typename>
class basic_snapshot;

/*! @class basic_snapshot_loader */
template<typename>
class basic_snapshot_loader;

/*! @class basic_continuous_loader */
template<typename>
class basic_continuous_loader;

/*! @enum entity */
enum class entity: id_type;

/*! @brief Alias declaration for the most common use case. */
using registry = basic_registry<entity>;

/*! @brief Alias declaration for the most common use case. */
using observer = basic_observer<entity>;

/*! @brief Alias declaration for the most common use case. */
using actor [[deprecated("Consider using the handle class instead")]] = basic_actor<entity>;

/*! @brief Alias declaration for the most common use case. */
using snapshot = basic_snapshot<entity>;

/*! @brief Alias declaration for the most common use case. */
using snapshot_loader = basic_snapshot_loader<entity>;

/*! @brief Alias declaration for the most common use case. */
using continuous_loader = basic_continuous_loader<entity>;

/**
 * @brief Alias declaration for the most common use case.
 * @tparam Types Types of components iterated by the view.
 */
template<typename... Types>
using view = basic_view<entity, Types...>;

/*! @brief Alias declaration for the most common use case. */
using runtime_view = basic_runtime_view<entity>;

/**
 * @brief Alias declaration for the most common use case.
 * @tparam Types Types of components iterated by the group.
 */
template<typename... Types>
using group = basic_group<entity, Types...>;


}


#endif
