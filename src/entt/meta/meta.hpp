#ifndef ENTT_META_META_HPP
#define ENTT_META_META_HPP


#include <tuple>
#include <array>
#include <memory>
#include <cstring>
#include <cassert>
#include <cstddef>
#include <utility>
#include <type_traits>
#include "../core/hashed_string.hpp"


namespace entt {


class meta_any;
class meta_handle;
class meta_prop;
class meta_base;
class meta_conv;
class meta_ctor;
class meta_dtor;
class meta_data;
class meta_func;
class meta_type;


/**
 * @cond TURN_OFF_DOXYGEN
 * Internal details not to be documented.
 */


namespace internal {


struct meta_type_node;


struct meta_prop_node final {
    meta_prop_node * const next;
    meta_any(* const key)();
    meta_any(* const value)();
};


struct meta_base_node final {
    meta_base_node * const next;
    meta_type_node * const parent;
    meta_type_node *(* const type)();
    void *(* const cast)(void *);
};


struct meta_conv_node final {
    meta_conv_node * const next;
    meta_type_node * const parent;
    meta_type_node *(* const type)();
    meta_any(* const conv)(void *);
};


struct meta_ctor_node final {
    using size_type = std::size_t;
    meta_ctor_node * const next;
    meta_type_node * const parent;
    meta_prop_node * const prop;
    const size_type size;
    meta_type_node *(* const arg)(size_type);
    meta_any(* const invoke)(meta_any * const);
};


struct meta_dtor_node final {
    meta_type_node * const parent;
    bool(* const invoke)(meta_handle);
};


struct meta_data_node final {
    const hashed_string name;
    meta_data_node * const next;
    meta_type_node * const parent;
    meta_prop_node * const prop;
    const bool is_const;
    const bool is_static;
    meta_type_node *(* const type)();
    bool(* const set)(meta_handle, meta_any &);
    meta_any(* const get)(meta_handle);
};


struct meta_func_node final {
    using size_type = std::size_t;
    const hashed_string name;
    meta_func_node * const next;
    meta_type_node * const parent;
    meta_prop_node * const prop;
    const size_type size;
    const bool is_const;
    const bool is_static;
    meta_type_node *(* const ret)();
    meta_type_node *(* const arg)(size_type);
    meta_any(* const invoke)(meta_handle, meta_any *);
};


struct meta_type_node final {
    const hashed_string name;
    meta_type_node * const next;
    meta_prop_node * const prop;
    const bool is_void;
    const bool is_enum;
    const bool is_class;
    const bool is_pointer;
    const bool is_function_pointer;
    const bool is_member_object_pointer;
    const bool is_member_function_pointer;
    const bool is_member_pointer;
    const bool is_arithmetic;
    const bool is_compound;
    bool(* const destroy)(meta_handle);
    meta_base_node *base;
    meta_conv_node *conv;
    meta_ctor_node *ctor;
    meta_dtor_node *dtor;
    meta_data_node *data;
    meta_func_node *func;
};


template<typename...>
struct meta_node {
    static meta_type_node *type;
};


template<typename... Type>
meta_type_node * meta_node<Type...>::type = nullptr;


template<typename Type>
struct meta_node<Type> {
    static meta_type_node *type;

    template<typename>
    static meta_base_node *base;

    template<typename>
    static meta_conv_node *conv;

    template<typename>
    static meta_ctor_node *ctor;

    template<auto>
    static meta_dtor_node *dtor;

    template<auto>
    static meta_data_node *data;

    template<auto>
    static meta_func_node *func;

    static meta_type_node * resolve() ENTT_NOEXCEPT;
};


template<typename Type>
meta_type_node * meta_node<Type>::type = nullptr;


template<typename Type>
template<typename>
meta_base_node * meta_node<Type>::base = nullptr;


template<typename Type>
template<typename>
meta_conv_node * meta_node<Type>::conv = nullptr;


template<typename Type>
template<typename>
meta_ctor_node * meta_node<Type>::ctor = nullptr;


template<typename Type>
template<auto>
meta_dtor_node * meta_node<Type>::dtor = nullptr;


template<typename Type>
template<auto>
meta_data_node * meta_node<Type>::data = nullptr;


template<typename Type>
template<auto>
meta_func_node * meta_node<Type>::func = nullptr;


template<typename... Type>
struct meta_info: meta_node<std::remove_cv_t<std::remove_reference_t<Type>>...> {};


template<typename Op, typename Node>
void iterate(Op op, const Node *curr) ENTT_NOEXCEPT {
    while(curr) {
        op(curr);
        curr = curr->next;
    }
}


template<auto Member, typename Op>
void iterate(Op op, const meta_type_node *node) ENTT_NOEXCEPT {
    if(node) {
        auto *curr = node->base;
        iterate(op, node->*Member);

        while(curr) {
            iterate<Member>(op, curr->type());
            curr = curr->next;
        }
    }
}


template<typename Op, typename Node>
auto find_if(Op op, const Node *curr) ENTT_NOEXCEPT {
    while(curr && !op(curr)) {
        curr = curr->next;
    }

    return curr;
}


template<auto Member, typename Op>
auto find_if(Op op, const meta_type_node *node) ENTT_NOEXCEPT
-> decltype(find_if(op, node->*Member))
{
    decltype(find_if(op, node->*Member)) ret = nullptr;

    if(node) {
        ret = find_if(op, node->*Member);
        auto *curr = node->base;

        while(curr && !ret) {
            ret = find_if<Member>(op, curr->type());
            curr = curr->next;
        }
    }

    return ret;
}


template<typename Type>
const Type * try_cast(const meta_type_node *node, void *instance) ENTT_NOEXCEPT {
    const auto *type = meta_info<Type>::resolve();
    void *ret = nullptr;

    if(node == type) {
        ret = instance;
    } else {
        const auto *base = find_if<&meta_type_node::base>([type](auto *node) {
            return node->type() == type;
        }, node);

        ret = base ? base->cast(instance) : nullptr;
    }

    return static_cast<const Type *>(ret);
}


template<auto Member>
inline bool can_cast_or_convert(const meta_type_node *from, const meta_type_node *to) ENTT_NOEXCEPT {
    return (from == to) || find_if<Member>([to](auto *node) {
        return node->type() == to;
    }, from);
}


template<typename... Args, std::size_t... Indexes>
inline auto ctor(std::index_sequence<Indexes...>, const meta_type_node *node) ENTT_NOEXCEPT {
    return internal::find_if([](auto *node) {
        return node->size == sizeof...(Args) &&
                (([](auto *from, auto *to) {
                    return internal::can_cast_or_convert<&internal::meta_type_node::base>(from, to)
                            || internal::can_cast_or_convert<&internal::meta_type_node::conv>(from, to);
                }(internal::meta_info<Args>::resolve(), node->arg(Indexes))) && ...);
    }, node->ctor);
}


template<typename>
struct factory_traits;


template<> struct factory_traits<meta_prop_node> { using type = meta_prop; };
template<> struct factory_traits<meta_base_node> { using type = meta_base; };
template<> struct factory_traits<meta_conv_node> { using type = meta_conv; };
template<> struct factory_traits<meta_ctor_node> { using type = meta_ctor; };
template<> struct factory_traits<meta_dtor_node> { using type = meta_dtor; };
template<> struct factory_traits<meta_data_node> { using type = meta_data; };
template<> struct factory_traits<meta_func_node> { using type = meta_func; };
template<> struct factory_traits<meta_type_node> { using type = meta_type; };


template<typename Node>
inline typename factory_traits<Node>::type factory(const Node *node) ENTT_NOEXCEPT {
    return node;
}


}


/**
 * Internal details not to be documented.
 * @endcond TURN_OFF_DOXYGEN
 */


/**
 * @brief Meta any object.
 *
 * A meta any is an opaque container for single values of any type.
 *
 * This class uses a technique called small buffer optimization (SBO) to
 * completely eliminate the need to allocate memory, where possible.<br/>
 * From the user's point of view, nothing will change, but the elimination of
 * allocations will reduce the jumps in memory and therefore will avoid chasing
 * of pointers. This will greatly improve the use of the cache, thus increasing
 * the overall performance.
 */
class meta_any final {
    /*! @brief A meta handle is allowed to _inherit_ from a meta any. */
    friend class meta_handle;

    using storage_type = std::aligned_storage_t<sizeof(void *), alignof(void *)>;
    using compare_fn_type = bool(*)(const void *, const void *);
    using copy_fn_type = void *(*)(storage_type &, const void *);
    using destroy_fn_type = void(*)(storage_type &);

    template<typename Type>
    inline static auto compare(int, const Type &lhs, const Type &rhs)
    -> decltype(lhs == rhs, bool{})
    {
        return lhs == rhs;
    }

    template<typename Type>
    inline static bool compare(char, const Type &lhs, const Type &rhs) {
        return &lhs == &rhs;
    }

public:
    /*! @brief Default constructor. */
    meta_any() ENTT_NOEXCEPT
        : storage{},
          instance{nullptr},
          destroy{nullptr},
          node{nullptr},
          comparator{nullptr}
    {}

    /**
     * @brief Constructs a meta any from a given value.
     *
     * This class uses a technique called small buffer optimization (SBO) to
     * completely eliminate the need to allocate memory, where possible.<br/>
     * From the user's point of view, nothing will change, but the elimination
     * of allocations will reduce the jumps in memory and therefore will avoid
     * chasing of pointers. This will greatly improve the use of the cache, thus
     * increasing the overall performance.
     *
     * @tparam Type Type of object to use to initialize the container.
     * @param type An instance of an object to use to initialize the container.
     */
    template<typename Type, typename = std::enable_if_t<!std::is_same_v<std::decay_t<Type>, meta_any>>>
    meta_any(Type &&type) {
        using actual_type = std::decay_t<Type>;
        node = internal::meta_info<Type>::resolve();

        comparator = [](const void *lhs, const void *rhs) {
            return compare(0, *static_cast<const actual_type *>(lhs), *static_cast<const actual_type *>(rhs));
        };

        if constexpr(sizeof(actual_type) <= sizeof(void *)) {
            new (&storage) actual_type{std::forward<Type>(type)};
            instance = &storage;

            copy = [](storage_type &storage, const void *instance) -> void * {
                return new (&storage) actual_type{*static_cast<const actual_type *>(instance)};
            };

            destroy = [](storage_type &storage) {
                auto *node = internal::meta_info<Type>::resolve();
                auto *instance = reinterpret_cast<actual_type *>(&storage);
                node->dtor ? node->dtor->invoke(*instance) : node->destroy(*instance);
            };
        } else {
            using chunk_type = std::aligned_storage_t<sizeof(actual_type), alignof(actual_type)>;
            auto *chunk = new chunk_type;
            instance = new (chunk) actual_type{std::forward<Type>(type)};
            new (&storage) chunk_type *{chunk};

            copy = [](storage_type &storage, const void *instance) -> void * {
                auto *chunk = new chunk_type;
                new (&storage) chunk_type *{chunk};
                return new (chunk) actual_type{*static_cast<const actual_type *>(instance)};
            };

            destroy = [](storage_type &storage) {
                auto *node = internal::meta_info<Type>::resolve();
                auto *chunk = *reinterpret_cast<chunk_type **>(&storage);
                auto *instance = reinterpret_cast<actual_type *>(chunk);
                node->dtor ? node->dtor->invoke(*instance) : node->destroy(*instance);
                delete chunk;
            };
        }
    }

    /**
     * @brief Copy constructor.
     * @param other The instance to copy from.
     */
    meta_any(const meta_any &other)
        : meta_any{}
    {
        if(other) {
            instance = other.copy(storage, other.instance);
            destroy = other.destroy;
            node = other.node;
            comparator = other.comparator;
            copy = other.copy;
        }
    }

    /**
     * @brief Move constructor.
     *
     * After meta any move construction, instances that have been moved from
     * are placed in a valid but unspecified state. It's highly discouraged to
     * continue using them.
     *
     * @param other The instance to move from.
     */
    meta_any(meta_any &&other) ENTT_NOEXCEPT
        : meta_any{}
    {
        swap(*this, other);
    }

    /*! @brief Frees the internal storage, whatever it means. */
    ~meta_any() {
        if(destroy) {
            destroy(storage);
        }
    }

    /**
     * @brief Assignment operator.
     * @param other The instance to assign.
     * @return This meta any object.
     */
    meta_any & operator=(meta_any other) {
        swap(*this, other);
        return *this;
    }

    /**
     * @brief Returns the meta type of the underlying object.
     * @return The meta type of the underlying object, if any.
     */
    inline meta_type type() const ENTT_NOEXCEPT;

    /**
     * @brief Returns an opaque pointer to the contained instance.
     * @return An opaque pointer the contained instance, if any.
     */
    inline const void * data() const ENTT_NOEXCEPT {
        return instance;
    }

    /**
     * @brief Returns an opaque pointer to the contained instance.
     * @return An opaque pointer the contained instance, if any.
     */
    inline void * data() ENTT_NOEXCEPT {
        return const_cast<void *>(std::as_const(*this).data());
    }

    /**
     * @brief Checks if it's possible to cast an instance to a given type.
     * @tparam Type Type to which to cast the instance.
     * @return True if the cast is viable, false otherwise.
     */
    template<typename Type>
    inline bool can_cast() const ENTT_NOEXCEPT {
        const auto *type = internal::meta_info<Type>::resolve();
        return internal::can_cast_or_convert<&internal::meta_type_node::base>(node, type);
    }

    /**
     * @brief Tries to cast an instance to a given type.
     *
     * The type of the instance must be such that the cast is possible.
     *
     * @warning
     * Attempting to perform a cast that isn't viable results in undefined
     * behavior.<br/>
     * An assertion will abort the execution at runtime in debug mode in case
     * the cast is not feasible.
     *
     * @tparam Type Type to which to cast the instance.
     * @return A reference to the contained instance.
     */
    template<typename Type>
    inline const Type & cast() const ENTT_NOEXCEPT {
        assert(can_cast<Type>());
        return *internal::try_cast<Type>(node, instance);
    }

    /**
     * @brief Tries to cast an instance to a given type.
     *
     * The type of the instance must be such that the cast is possible.
     *
     * @warning
     * Attempting to perform a cast that isn't viable results in undefined
     * behavior.<br/>
     * An assertion will abort the execution at runtime in debug mode in case
     * the cast is not feasible.
     *
     * @tparam Type Type to which to cast the instance.
     * @return A reference to the contained instance.
     */
    template<typename Type>
    inline Type & cast() ENTT_NOEXCEPT {
        return const_cast<Type &>(std::as_const(*this).cast<Type>());
    }

    /**
     * @brief Checks if it's possible to convert an instance to a given type.
     * @tparam Type Type to which to convert the instance.
     * @return True if the conversion is viable, false otherwise.
     */
    template<typename Type>
    inline bool can_convert() const ENTT_NOEXCEPT {
        const auto *type = internal::meta_info<Type>::resolve();
        return internal::can_cast_or_convert<&internal::meta_type_node::conv>(node, type);
    }

    /**
     * @brief Tries to convert an instance to a given type and returns it.
     * @tparam Type Type to which to convert the instance.
     * @return A valid meta any object if the conversion is possible, an invalid
     * one otherwise.
     */
    template<typename Type>
    inline meta_any convert() const ENTT_NOEXCEPT {
        const auto *type = internal::meta_info<Type>::resolve();
        meta_any any{};

        if(node == type) {
            any = *static_cast<const Type *>(instance);
        } else {
            const auto *conv = internal::find_if<&internal::meta_type_node::conv>([type](auto *node) {
                return node->type() == type;
            }, node);

            if(conv) {
                any = conv->conv(instance);
            }
        }

        return any;
    }

    /**
     * @brief Tries to convert an instance to a given type.
     * @tparam Type Type to which to convert the instance.
     * @return True if the conversion is possible, false otherwise.
     */
    template<typename Type>
    inline bool convert() ENTT_NOEXCEPT {
        bool valid = (node == internal::meta_info<Type>::resolve());

        if(!valid) {
            auto any = std::as_const(*this).convert<Type>();

            if(any) {
                std::swap(*this, any);
                valid = true;
            }
        }

        return valid;
    }

    /**
     * @brief Returns false if a container is empty, true otherwise.
     * @return False if the container is empty, true otherwise.
     */
    inline explicit operator bool() const ENTT_NOEXCEPT {
        return destroy;
    }

    /**
     * @brief Checks if two containers differ in their content.
     * @param other Container with which to compare.
     * @return False if the two containers differ in their content, true
     * otherwise.
     */
    inline bool operator==(const meta_any &other) const ENTT_NOEXCEPT {
        return (!instance && !other.instance) || (instance && other.instance && node == other.node && comparator(instance, other.instance));
    }

    /**
     * @brief Swaps two meta any objects.
     * @param lhs A valid meta any object.
     * @param rhs A valid meta any object.
     */
    friend void swap(meta_any &lhs, meta_any &rhs) {
        using std::swap;

        std::swap(lhs.storage, rhs.storage);
        std::swap(lhs.instance, rhs.instance);
        std::swap(lhs.destroy, rhs.destroy);
        std::swap(lhs.node, rhs.node);
        std::swap(lhs.comparator, rhs.comparator);
        std::swap(lhs.copy, rhs.copy);

        if(lhs.instance == &rhs.storage) {
            lhs.instance = &lhs.storage;
        }

        if(rhs.instance == &lhs.storage) {
            rhs.instance = &rhs.storage;
        }
    }

private:
    storage_type storage;
    void *instance;
    destroy_fn_type destroy;
    internal::meta_type_node *node;
    compare_fn_type comparator;
    copy_fn_type copy;
};


/**
 * @brief Meta handle object.
 *
 * A meta handle is an opaque pointer to an instance of any type.
 *
 * A handle doesn't perform copies and isn't responsible for the contained
 * object. It doesn't prolong the lifetime of the pointed instance. Users are
 * responsible for ensuring that the target object remains alive for the entire
 * interval of use of the handle.
 */
class meta_handle final {
    meta_handle(int, meta_any &any) ENTT_NOEXCEPT
        : node{any.node},
          instance{any.instance}
    {}

    template<typename Type>
    meta_handle(char, Type &&instance) ENTT_NOEXCEPT
        : node{internal::meta_info<Type>::resolve()},
          instance{&instance}
    {}

public:
    /*! @brief Default constructor. */
    meta_handle() ENTT_NOEXCEPT
        : node{nullptr},
          instance{nullptr}
    {}

    /**
     * @brief Constructs a meta handle from a given instance.
     * @tparam Type Type of object to use to initialize the handle.
     * @param instance A reference to an object to use to initialize the handle.
     */
    template<typename Type, typename = std::enable_if_t<!std::is_same_v<std::decay_t<Type>, meta_handle>>>
    meta_handle(Type &&instance) ENTT_NOEXCEPT
        : meta_handle{0, std::forward<Type>(instance)}
    {}

    /*! @brief Default destructor. */
    ~meta_handle() ENTT_NOEXCEPT = default;

    /*! @brief Default copy constructor. */
    meta_handle(const meta_handle &) ENTT_NOEXCEPT = default;

    /*! @brief Default move constructor. */
    meta_handle(meta_handle &&) ENTT_NOEXCEPT = default;

    /*! @brief Default copy assignment operator. @return This handle. */
    meta_handle & operator=(const meta_handle &) ENTT_NOEXCEPT = default;

    /*! @brief Default move assignment operator. @return This handle. */
    meta_handle & operator=(meta_handle &&) ENTT_NOEXCEPT = default;

    /**
     * @brief Returns the meta type of the underlying object.
     * @return The meta type of the underlying object, if any.
     */
    inline meta_type type() const ENTT_NOEXCEPT;

    /**
     * @brief Tries to cast an instance to a given type.
     *
     * The type of the instance must be such that the conversion is possible.
     *
     * @warning
     * Attempting to perform a conversion that isn't viable results in undefined
     * behavior.<br/>
     * An assertion will abort the execution at runtime in debug mode in case
     * the conversion is not feasible.
     *
     * @tparam Type Type to which to cast the instance.
     * @return A pointer to the contained instance.
     */
    template<typename Type>
    inline const Type * try_cast() const ENTT_NOEXCEPT {
        return internal::try_cast<Type>(node, instance);
    }

    /**
     * @brief Tries to cast an instance to a given type.
     *
     * The type of the instance must be such that the conversion is possible.
     *
     * @warning
     * Attempting to perform a conversion that isn't viable results in undefined
     * behavior.<br/>
     * An assertion will abort the execution at runtime in debug mode in case
     * the conversion is not feasible.
     *
     * @tparam Type Type to which to cast the instance.
     * @return A pointer to the contained instance.
     */
    template<typename Type>
    inline Type * try_cast() ENTT_NOEXCEPT {
        return const_cast<Type *>(std::as_const(*this).try_cast<Type>());
    }

    /**
     * @brief Returns an opaque pointer to the contained instance.
     * @return An opaque pointer the contained instance, if any.
     */
    inline const void * data() const ENTT_NOEXCEPT {
        return instance;
    }

    /**
     * @brief Returns an opaque pointer to the contained instance.
     * @return An opaque pointer the contained instance, if any.
     */
    inline void * data() ENTT_NOEXCEPT {
        return const_cast<void *>(std::as_const(*this).data());
    }

    /**
     * @brief Returns false if a handle is empty, true otherwise.
     * @return False if the handle is empty, true otherwise.
     */
    inline explicit operator bool() const ENTT_NOEXCEPT {
        return instance;
    }

private:
    const internal::meta_type_node *node;
    void *instance;
};


/**
 * @brief Checks if two containers differ in their content.
 * @param lhs A meta any object, either empty or not.
 * @param rhs A meta any object, either empty or not.
 * @return True if the two containers differ in their content, false otherwise.
 */
inline bool operator!=(const meta_any &lhs, const meta_any &rhs) ENTT_NOEXCEPT {
    return !(lhs == rhs);
}


/**
 * @brief Meta property object.
 *
 * A meta property is an opaque container for a key/value pair.<br/>
 * Properties are associated with any other meta object to enrich it.
 */
class meta_prop final {
    /*! @brief A factory is allowed to create meta objects. */
    friend meta_prop internal::factory<internal::meta_prop_node>(const internal::meta_prop_node *) ENTT_NOEXCEPT;

    inline meta_prop(const internal::meta_prop_node *node) ENTT_NOEXCEPT
        : node{node}
    {}

public:
    /**
     * @brief Returns the stored key.
     * @return A meta any containing the key stored with the given property.
     */
    inline meta_any key() const ENTT_NOEXCEPT {
        return node->key();
    }

    /**
     * @brief Returns the stored value.
     * @return A meta any containing the value stored with the given property.
     */
    inline meta_any value() const ENTT_NOEXCEPT {
        return node->value();
    }

    /**
     * @brief Returns true if a meta object is valid, false otherwise.
     * @return True if the meta object is valid, false otherwise.
     */
    inline explicit operator bool() const ENTT_NOEXCEPT {
        return node;
    }

    /**
     * @brief Checks if two meta objects refer to the same node.
     * @param other The meta object with which to compare.
     * @return True if the two meta objects refer to the same node, false
     * otherwise.
     */
    inline bool operator==(const meta_prop &other) const ENTT_NOEXCEPT {
        return node == other.node;
    }

private:
    const internal::meta_prop_node *node;
};


/**
 * @brief Checks if two meta objects refer to the same node.
 * @param lhs A meta object, either valid or not.
 * @param rhs A meta object, either valid or not.
 * @return True if the two meta objects refer to the same node, false otherwise.
 */
inline bool operator!=(const meta_prop &lhs, const meta_prop &rhs) ENTT_NOEXCEPT {
    return !(lhs == rhs);
}


/**
 * @brief Meta base object.
 *
 * A meta base is an opaque container for a base class to be used to walk
 * through hierarchies.
 */
class meta_base final {
    /*! @brief A factory is allowed to create meta objects. */
    friend meta_base internal::factory<internal::meta_base_node>(const internal::meta_base_node *) ENTT_NOEXCEPT;

    inline meta_base(const internal::meta_base_node *node) ENTT_NOEXCEPT
        : node{node}
    {}

public:
    /**
     * @brief Returns the meta type to which a meta base belongs.
     * @return The meta type to which the meta base belongs.
     */
    inline meta_type parent() const ENTT_NOEXCEPT;

    /**
     * @brief Returns the meta type of a given meta base.
     * @return The meta type of the meta base.
     */
    inline meta_type type() const ENTT_NOEXCEPT;

    /**
     * @brief Casts an instance from a parent type to a base type.
     * @param instance The instance to cast.
     * @return An opaque pointer to the base type.
     */
    inline void * cast(void *instance) const ENTT_NOEXCEPT {
        return node->cast(instance);
    }

    /**
     * @brief Returns true if a meta object is valid, false otherwise.
     * @return True if the meta object is valid, false otherwise.
     */
    inline explicit operator bool() const ENTT_NOEXCEPT {
        return node;
    }

    /**
     * @brief Checks if two meta objects refer to the same node.
     * @param other The meta object with which to compare.
     * @return True if the two meta objects refer to the same node, false
     * otherwise.
     */
    inline bool operator==(const meta_base &other) const ENTT_NOEXCEPT {
        return node == other.node;
    }

private:
    const internal::meta_base_node *node;
};


/**
 * @brief Checks if two meta objects refer to the same node.
 * @param lhs A meta object, either valid or not.
 * @param rhs A meta object, either valid or not.
 * @return True if the two meta objects refer to the same node, false otherwise.
 */
inline bool operator!=(const meta_base &lhs, const meta_base &rhs) ENTT_NOEXCEPT {
    return !(lhs == rhs);
}


/**
 * @brief Meta conversion function object.
 *
 * A meta conversion function is an opaque container for a conversion function
 * to be used to convert a given instance to another type.
 */
class meta_conv final {
    /*! @brief A factory is allowed to create meta objects. */
    friend meta_conv internal::factory<internal::meta_conv_node>(const internal::meta_conv_node *) ENTT_NOEXCEPT;

    inline meta_conv(const internal::meta_conv_node *node) ENTT_NOEXCEPT
        : node{node}
    {}

public:
    /**
     * @brief Returns the meta type to which a meta conversion function belongs.
     * @return The meta type to which the meta conversion function belongs.
     */
    inline meta_type parent() const ENTT_NOEXCEPT;

    /**
     * @brief Returns the meta type of a given meta conversion function.
     * @return The meta type of the meta conversion function.
     */
    inline meta_type type() const ENTT_NOEXCEPT;

    /**
     * @brief Converts an instance to a given type.
     * @param instance The instance to convert.
     * @return An opaque pointer to the instance to convert.
     */
    inline meta_any convert(void *instance) const ENTT_NOEXCEPT {
        return node->conv(instance);
    }

    /**
     * @brief Returns true if a meta object is valid, false otherwise.
     * @return True if the meta object is valid, false otherwise.
     */
    inline explicit operator bool() const ENTT_NOEXCEPT {
        return node;
    }

    /**
     * @brief Checks if two meta objects refer to the same node.
     * @param other The meta object with which to compare.
     * @return True if the two meta objects refer to the same node, false
     * otherwise.
     */
    inline bool operator==(const meta_conv &other) const ENTT_NOEXCEPT {
        return node == other.node;
    }

private:
    const internal::meta_conv_node *node;
};


/**
 * @brief Checks if two meta objects refer to the same node.
 * @param lhs A meta object, either valid or not.
 * @param rhs A meta object, either valid or not.
 * @return True if the two meta objects refer to the same node, false otherwise.
 */
inline bool operator!=(const meta_conv &lhs, const meta_conv &rhs) ENTT_NOEXCEPT {
    return !(lhs == rhs);
}


/**
 * @brief Meta constructor object.
 *
 * A meta constructor is an opaque container for a function to be used to
 * construct instances of a given type.
 */
class meta_ctor final {
    /*! @brief A factory is allowed to create meta objects. */
    friend meta_ctor internal::factory<internal::meta_ctor_node>(const internal::meta_ctor_node *) ENTT_NOEXCEPT;

    inline meta_ctor(const internal::meta_ctor_node *node) ENTT_NOEXCEPT
        : node{node}
    {}

public:
    /*! @brief Unsigned integer type. */
    using size_type = typename internal::meta_ctor_node::size_type;

    /**
     * @brief Returns the meta type to which a meta constructor belongs.
     * @return The meta type to which the meta constructor belongs.
     */
    inline meta_type parent() const ENTT_NOEXCEPT;

    /**
     * @brief Returns the number of arguments accepted by a meta constructor.
     * @return The number of arguments accepted by the meta constructor.
     */
    inline size_type size() const ENTT_NOEXCEPT {
        return node->size;
    }

    /**
     * @brief Returns the meta type of the i-th argument of a meta constructor.
     * @param index The index of the argument of which to return the meta type.
     * @return The meta type of the i-th argument of a meta constructor, if any.
     */
    inline meta_type arg(size_type index) const ENTT_NOEXCEPT;

    /**
     * @brief Creates an instance of the underlying type, if possible.
     *
     * To create a valid instance, the types of the parameters must coincide
     * exactly with those required by the underlying meta constructor.
     * Otherwise, an empty and then invalid container is returned.
     *
     * @tparam Args Types of arguments to use to construct the instance.
     * @param args Parameters to use to construct the instance.
     * @return A meta any containing the new instance, if any.
     */
    template<typename... Args>
    meta_any invoke(Args &&... args) const {
        std::array<meta_any, sizeof...(Args)> arguments{{std::forward<Args>(args)...}};
        meta_any any{};

        if(sizeof...(Args) == size()) {
            any = node->invoke(arguments.data());
        }

        return any;
    }

    /**
     * @brief Iterates all the properties assigned to a meta constructor.
     * @tparam Op Type of the function object to invoke.
     * @param op A valid function object.
     */
    template<typename Op>
    inline std::enable_if_t<std::is_invocable_v<Op, meta_prop>, void>
    prop(Op op) const ENTT_NOEXCEPT {
        internal::iterate([op = std::move(op)](auto *node) {
            op(internal::factory(node));
        }, node->prop);
    }

    /**
     * @brief Returns the property associated with a given key.
     * @tparam Key Type of key to use to search for a property.
     * @param key The key to use to search for a property.
     * @return The property associated with the given key, if any.
     */
    template<typename Key>
    inline std::enable_if_t<!std::is_invocable_v<Key, meta_prop>, meta_prop>
    prop(Key &&key) const ENTT_NOEXCEPT {
        return internal::factory(internal::find_if([key = meta_any{std::forward<Key>(key)}](auto *curr) {
            return curr->key() == key;
        }, node->prop));
    }

    /**
     * @brief Returns true if a meta object is valid, false otherwise.
     * @return True if the meta object is valid, false otherwise.
     */
    inline explicit operator bool() const ENTT_NOEXCEPT {
        return node;
    }

    /**
     * @brief Checks if two meta objects refer to the same node.
     * @param other The meta object with which to compare.
     * @return True if the two meta objects refer to the same node, false
     * otherwise.
     */
    inline bool operator==(const meta_ctor &other) const ENTT_NOEXCEPT {
        return node == other.node;
    }

private:
    const internal::meta_ctor_node *node;
};


/**
 * @brief Checks if two meta objects refer to the same node.
 * @param lhs A meta object, either valid or not.
 * @param rhs A meta object, either valid or not.
 * @return True if the two meta objects refer to the same node, false otherwise.
 */
inline bool operator!=(const meta_ctor &lhs, const meta_ctor &rhs) ENTT_NOEXCEPT {
    return !(lhs == rhs);
}


/**
 * @brief Meta destructor object.
 *
 * A meta destructor is an opaque container for a function to be used to
 * destroy instances of a given type.
 */
class meta_dtor final {
    /*! @brief A factory is allowed to create meta objects. */
    friend meta_dtor internal::factory<internal::meta_dtor_node>(const internal::meta_dtor_node *) ENTT_NOEXCEPT;

    inline meta_dtor(const internal::meta_dtor_node *node) ENTT_NOEXCEPT
        : node{node}
    {}

public:
    /**
     * @brief Returns the meta type to which a meta destructor belongs.
     * @return The meta type to which the meta destructor belongs.
     */
    inline meta_type parent() const ENTT_NOEXCEPT;

    /**
     * @brief Destroys an instance of the underlying type.
     *
     * It must be possible to cast the instance to the parent type of the meta
     * destructor. Otherwise, invoking the meta destructor results in an
     * undefined behavior.
     *
     * @param handle An opaque pointer to an instance of the underlying type.
     * @return True in case of success, false otherwise.
     */
    inline bool invoke(meta_handle handle) const {
        return node->invoke(handle);
    }

    /**
     * @brief Returns true if a meta object is valid, false otherwise.
     * @return True if the meta object is valid, false otherwise.
     */
    inline explicit operator bool() const ENTT_NOEXCEPT {
        return node;
    }

    /**
     * @brief Checks if two meta objects refer to the same node.
     * @param other The meta object with which to compare.
     * @return True if the two meta objects refer to the same node, false
     * otherwise.
     */
    inline bool operator==(const meta_dtor &other) const ENTT_NOEXCEPT {
        return node == other.node;
    }

private:
    const internal::meta_dtor_node *node;
};


/**
 * @brief Checks if two meta objects refer to the same node.
 * @param lhs A meta object, either valid or not.
 * @param rhs A meta object, either valid or not.
 * @return True if the two meta objects refer to the same node, false otherwise.
 */
inline bool operator!=(const meta_dtor &lhs, const meta_dtor &rhs) ENTT_NOEXCEPT {
    return !(lhs == rhs);
}


/**
 * @brief Meta data object.
 *
 * A meta data is an opaque container for a data member associated with a given
 * type.
 */
class meta_data final {
    /*! @brief A factory is allowed to create meta objects. */
    friend meta_data internal::factory<internal::meta_data_node>(const internal::meta_data_node *) ENTT_NOEXCEPT;

    inline meta_data(const internal::meta_data_node *node) ENTT_NOEXCEPT
        : node{node}
    {}

public:
    /**
     * @brief Returns the name assigned to a given meta data.
     * @return The name assigned to the meta data.
     */
    inline const char * name() const ENTT_NOEXCEPT {
        return node->name;
    }

    /**
     * @brief Returns the meta type to which a meta data belongs.
     * @return The meta type to which the meta data belongs.
     */
    inline meta_type parent() const ENTT_NOEXCEPT;

    /**
     * @brief Indicates whether a given meta data is constant or not.
     * @return True if the meta data is constant, false otherwise.
     */
    inline bool is_const() const ENTT_NOEXCEPT {
        return node->is_const;
    }

    /**
     * @brief Indicates whether a given meta data is static or not.
     *
     * A static meta data is such that it can be accessed using a null pointer
     * as an instance.
     *
     * @return True if the meta data is static, false otherwise.
     */
    inline bool is_static() const ENTT_NOEXCEPT {
        return node->is_static;
    }

    /**
     * @brief Returns the meta type of a given meta data.
     * @return The meta type of the meta data.
     */
    inline meta_type type() const ENTT_NOEXCEPT;

    /**
     * @brief Sets the value of the variable enclosed by a given meta type.
     *
     * It must be possible to cast the instance to the parent type of the meta
     * data. Otherwise, invoking the setter results in an undefined
     * behavior.<br/>
     * The type of the value must coincide exactly with that of the variable
     * enclosed by the meta data. Otherwise, invoking the setter does nothing.
     *
     * @tparam Type Type of value to assign.
     * @param handle An opaque pointer to an instance of the underlying type.
     * @param value Parameter to use to set the underlying variable.
     * @return True in case of success, false otherwise.
     */
    template<typename Type>
    inline bool set(meta_handle handle, Type &&value) const {
        meta_any any{std::forward<Type>(value)};
        return node->set(handle, any);
    }

    /**
     * @brief Gets the value of the variable enclosed by a given meta type.
     *
     * It must be possible to cast the instance to the parent type of the meta
     * function. Otherwise, invoking the getter results in an undefined
     * behavior.
     *
     * @param handle An opaque pointer to an instance of the underlying type.
     * @return A meta any containing the value of the underlying variable.
     */
    inline meta_any get(meta_handle handle) const ENTT_NOEXCEPT {
        return node->get(handle);
    }

    /**
     * @brief Iterates all the properties assigned to a meta data.
     * @tparam Op Type of the function object to invoke.
     * @param op A valid function object.
     */
    template<typename Op>
    inline std::enable_if_t<std::is_invocable_v<Op, meta_prop>, void>
    prop(Op op) const ENTT_NOEXCEPT {
        internal::iterate([op = std::move(op)](auto *node) {
            op(internal::factory(node));
        }, node->prop);
    }

    /**
     * @brief Returns the property associated with a given key.
     * @tparam Key Type of key to use to search for a property.
     * @param key The key to use to search for a property.
     * @return The property associated with the given key, if any.
     */
    template<typename Key>
    inline std::enable_if_t<!std::is_invocable_v<Key, meta_prop>, meta_prop>
    prop(Key &&key) const ENTT_NOEXCEPT {
        return internal::factory(internal::find_if([key = meta_any{std::forward<Key>(key)}](auto *curr) {
            return curr->key() == key;
        }, node->prop));
    }

    /**
     * @brief Returns true if a meta object is valid, false otherwise.
     * @return True if the meta object is valid, false otherwise.
     */
    inline explicit operator bool() const ENTT_NOEXCEPT {
        return node;
    }

    /**
     * @brief Checks if two meta objects refer to the same node.
     * @param other The meta object with which to compare.
     * @return True if the two meta objects refer to the same node, false
     * otherwise.
     */
    inline bool operator==(const meta_data &other) const ENTT_NOEXCEPT {
        return node == other.node;
    }

private:
    const internal::meta_data_node *node;
};


/**
 * @brief Checks if two meta objects refer to the same node.
 * @param lhs A meta object, either valid or not.
 * @param rhs A meta object, either valid or not.
 * @return True if the two meta objects refer to the same node, false otherwise.
 */
inline bool operator!=(const meta_data &lhs, const meta_data &rhs) ENTT_NOEXCEPT {
    return !(lhs == rhs);
}


/**
 * @brief Meta function object.
 *
 * A meta function is an opaque container for a member function associated with
 * a given type.
 */
class meta_func final {
    /*! @brief A factory is allowed to create meta objects. */
    friend meta_func internal::factory<internal::meta_func_node>(const internal::meta_func_node *) ENTT_NOEXCEPT;

    inline meta_func(const internal::meta_func_node *node) ENTT_NOEXCEPT
        : node{node}
    {}

public:
    /*! @brief Unsigned integer type. */
    using size_type = typename internal::meta_func_node::size_type;

    /**
     * @brief Returns the name assigned to a given meta function.
     * @return The name assigned to the meta function.
     */
    inline const char * name() const ENTT_NOEXCEPT {
        return node->name;
    }

    /**
     * @brief Returns the meta type to which a meta function belongs.
     * @return The meta type to which the meta function belongs.
     */
    inline meta_type parent() const ENTT_NOEXCEPT;

    /**
     * @brief Returns the number of arguments accepted by a meta function.
     * @return The number of arguments accepted by the meta function.
     */
    inline size_type size() const ENTT_NOEXCEPT {
        return node->size;
    }

    /**
     * @brief Indicates whether a given meta function is constant or not.
     * @return True if the meta function is constant, false otherwise.
     */
    inline bool is_const() const ENTT_NOEXCEPT {
        return node->is_const;
    }

    /**
     * @brief Indicates whether a given meta function is static or not.
     *
     * A static meta function is such that it can be invoked using a null
     * pointer as an instance.
     *
     * @return True if the meta function is static, false otherwise.
     */
    inline bool is_static() const ENTT_NOEXCEPT {
        return node->is_static;
    }

    /**
     * @brief Returns the meta type of the return type of a meta function.
     * @return The meta type of the return type of the meta function.
     */
    inline meta_type ret() const ENTT_NOEXCEPT;

    /**
     * @brief Returns the meta type of the i-th argument of a meta function.
     * @param index The index of the argument of which to return the meta type.
     * @return The meta type of the i-th argument of a meta function, if any.
     */
    inline meta_type arg(size_type index) const ENTT_NOEXCEPT;

    /**
     * @brief Invokes the underlying function, if possible.
     *
     * To invoke a meta function, the types of the parameters must coincide
     * exactly with those required by the underlying function. Otherwise, an
     * empty and then invalid container is returned.<br/>
     * It must be possible to cast the instance to the parent type of the meta
     * function. Otherwise, invoking the underlying function results in an
     * undefined behavior.
     *
     * @tparam Args Types of arguments to use to invoke the function.
     * @param handle An opaque pointer to an instance of the underlying type.
     * @param args Parameters to use to invoke the function.
     * @return A meta any containing the returned value, if any.
     */
    template<typename... Args>
    meta_any invoke(meta_handle handle, Args &&... args) const {
        std::array<meta_any, sizeof...(Args)> arguments{{std::forward<Args>(args)...}};
        meta_any any{};

        if(sizeof...(Args) == size()) {
            any = node->invoke(handle, arguments.data());
        }

        return any;
    }

    /**
     * @brief Iterates all the properties assigned to a meta function.
     * @tparam Op Type of the function object to invoke.
     * @param op A valid function object.
     */
    template<typename Op>
    inline std::enable_if_t<std::is_invocable_v<Op, meta_prop>, void>
    prop(Op op) const ENTT_NOEXCEPT {
        internal::iterate([op = std::move(op)](auto *node) {
            op(internal::factory(node));
        }, node->prop);
    }

    /**
     * @brief Returns the property associated with a given key.
     * @tparam Key Type of key to use to search for a property.
     * @param key The key to use to search for a property.
     * @return The property associated with the given key, if any.
     */
    template<typename Key>
    inline std::enable_if_t<!std::is_invocable_v<Key, meta_prop>, meta_prop>
    prop(Key &&key) const ENTT_NOEXCEPT {
        return internal::factory(internal::find_if([key = meta_any{std::forward<Key>(key)}](auto *curr) {
            return curr->key() == key;
        }, node->prop));
    }

    /**
     * @brief Returns true if a meta object is valid, false otherwise.
     * @return True if the meta object is valid, false otherwise.
     */
    inline explicit operator bool() const ENTT_NOEXCEPT {
        return node;
    }

    /**
     * @brief Checks if two meta objects refer to the same node.
     * @param other The meta object with which to compare.
     * @return True if the two meta objects refer to the same node, false
     * otherwise.
     */
    inline bool operator==(const meta_func &other) const ENTT_NOEXCEPT {
        return node == other.node;
    }

private:
    const internal::meta_func_node *node;
};


/**
 * @brief Checks if two meta objects refer to the same node.
 * @param lhs A meta object, either valid or not.
 * @param rhs A meta object, either valid or not.
 * @return True if the two meta objects refer to the same node, false otherwise.
 */
inline bool operator!=(const meta_func &lhs, const meta_func &rhs) ENTT_NOEXCEPT {
    return !(lhs == rhs);
}


/**
 * @brief Meta type object.
 *
 * A meta type is the starting point for accessing a reflected type, thus being
 * able to work through it on real objects.
 */
class meta_type final {
    /*! @brief A factory is allowed to create meta objects. */
    friend meta_type internal::factory<internal::meta_type_node>(const internal::meta_type_node *) ENTT_NOEXCEPT;

    inline meta_type(const internal::meta_type_node *node) ENTT_NOEXCEPT
        : node{node}
    {}

public:
    /**
     * @brief Returns the name assigned to a given meta type.
     * @return The name assigned to the meta type.
     */
    inline const char * name() const ENTT_NOEXCEPT {
        return node->name;
    }

    /**
     * @brief Indicates whether a given meta type refers to void or not.
     * @return True if the underlying type is void, false otherwise.
     */
    inline bool is_void() const ENTT_NOEXCEPT {
        return node->is_void;
    }

    /**
     * @brief Indicates whether a given meta type refers to an enum or not.
     * @return True if the underlying type is an enum, false otherwise.
     */
    inline bool is_enum() const ENTT_NOEXCEPT {
        return node->is_enum;
    }

    /**
     * @brief Indicates whether a given meta type refers to a class or not.
     * @return True if the underlying type is a class, false otherwise.
     */
    inline bool is_class() const ENTT_NOEXCEPT {
        return node->is_class;
    }

    /**
     * @brief Indicates whether a given meta type refers to a pointer or not.
     * @return True if the underlying type is a pointer, false otherwise.
     */
    inline bool is_pointer() const ENTT_NOEXCEPT {
        return node->is_pointer;
    }

    /**
     * @brief Indicates whether a given meta type refers to a function pointer
     * or not.
     * @return True if the underlying type is a function pointer, false
     * otherwise.
     */
    inline bool is_function_pointer() const ENTT_NOEXCEPT {
        return node->is_function_pointer;
    }

    /**
     * @brief Indicates whether a given meta type refers to a pointer to data
     * member or not.
     * @return True if the underlying type is a pointer to data member, false
     * otherwise.
     */
    inline bool is_member_object_pointer() const ENTT_NOEXCEPT {
        return node->is_member_object_pointer;
    }

    /**
     * @brief Indicates whether a given meta type refers to a pointer to member
     * function or not.
     * @return True if the underlying type is a pointer to member function,
     * false otherwise.
     */
    inline bool is_member_function_pointer() const ENTT_NOEXCEPT {
        return node->is_member_function_pointer;
    }

    /**
     * @brief Indicates whether a given meta type refers to a pointer to member
     * or not.
     * @return True if the underlying type is a pointer to member, false
     * otherwise.
     */
    inline bool is_member_pointer() const ENTT_NOEXCEPT {
        return node->is_member_pointer;
    }

    /**
     * @brief Indicates whether a given meta type refers to an arithmetic type
     * or not.
     * @return True if the underlying type is an arithmetic type, false
     * otherwise.
     */
    inline bool is_arithmetic() const ENTT_NOEXCEPT {
        return node->is_arithmetic;
    }

    /**
     * @brief Indicates whether a given meta type refers to a compound type or
     * not.
     * @return True if the underlying type is a compound type, false otherwise.
     */
    inline bool is_compound() const ENTT_NOEXCEPT {
        return node->is_compound;
    }

    /**
     * @brief Iterates all the meta base of a meta type.
     *
     * Iteratively returns **all** the base classes of the given type.
     *
     * @tparam Op Type of the function object to invoke.
     * @param op A valid function object.
     */
    template<typename Op>
    inline void base(Op op) const ENTT_NOEXCEPT {
        internal::iterate<&internal::meta_type_node::base>([op = std::move(op)](auto *node) {
            op(internal::factory(node));
        }, node);
    }

    /**
     * @brief Returns the meta base associated with a given name.
     *
     * Searches recursively among **all** the base classes of the given type.
     *
     * @param str The name to use to search for a meta base.
     * @return The meta base associated with the given name, if any.
     */
    inline meta_base base(const char *str) const ENTT_NOEXCEPT {
        return internal::factory(internal::find_if<&internal::meta_type_node::base>([name = hashed_string{str}](auto *node) {
            return node->type()->name == name;
        }, node));
    }

    /**
     * @brief Iterates all the meta conversion functions of a meta type.
     *
     * Iteratively returns **all** the meta conversion functions of the given
     * type.
     *
     * @tparam Op Type of the function object to invoke.
     * @param op A valid function object.
     */
    template<typename Op>
    inline void conv(Op op) const ENTT_NOEXCEPT {
        internal::iterate<&internal::meta_type_node::conv>([op = std::move(op)](auto *node) {
            op(internal::factory(node));
        }, node);
    }

    /**
     * @brief Returns the meta conversion function associated with a given type.
     *
     * Searches recursively among **all** the conversion functions of the given
     * type.
     *
     * @tparam Type The type to use to search for a meta conversion function.
     * @return The meta conversion function associated with the given type, if
     * any.
     */
    template<typename Type>
    inline meta_conv conv() const ENTT_NOEXCEPT {
        return internal::factory(internal::find_if<&internal::meta_type_node::conv>([type = internal::meta_info<Type>::resolve()](auto *node) {
            return node->type() == type;
        }, node));
    }

    /**
     * @brief Iterates all the meta constructors of a meta type.
     * @tparam Op Type of the function object to invoke.
     * @param op A valid function object.
     */
    template<typename Op>
    inline void ctor(Op op) const ENTT_NOEXCEPT {
        internal::iterate([op = std::move(op)](auto *node) {
            op(internal::factory(node));
        }, node->ctor);
    }

    /**
     * @brief Returns the meta constructor that accepts a given list of types of
     * arguments.
     * @return The requested meta constructor, if any.
     */
    template<typename... Args>
    inline meta_ctor ctor() const ENTT_NOEXCEPT {
        return internal::factory(internal::ctor<Args...>(std::make_index_sequence<sizeof...(Args)>{}, node));
    }

    /**
     * @brief Returns the meta destructor associated with a given type.
     * @return The meta destructor associated with the given type, if any.
     */
    inline meta_dtor dtor() const ENTT_NOEXCEPT {
        return internal::factory(node->dtor);
    }

    /**
     * @brief Iterates all the meta data of a meta type.
     *
     * Iteratively returns **all** the meta data of the given type. This means
     * that the meta data of the base classes will also be returned, if any.
     *
     * @tparam Op Type of the function object to invoke.
     * @param op A valid function object.
     */
    template<typename Op>
    inline void data(Op op) const ENTT_NOEXCEPT {
        internal::iterate<&internal::meta_type_node::data>([op = std::move(op)](auto *node) {
            op(internal::factory(node));
        }, node);
    }

    /**
     * @brief Returns the meta data associated with a given name.
     *
     * Searches recursively among **all** the meta data of the given type. This
     * means that the meta data of the base classes will also be inspected, if
     * any.
     *
     * @param str The name to use to search for a meta data.
     * @return The meta data associated with the given name, if any.
     */
    inline meta_data data(const char *str) const ENTT_NOEXCEPT {
        return internal::factory(internal::find_if<&internal::meta_type_node::data>([name = hashed_string{str}](auto *node) {
            return node->name == name;
        }, node));
    }

    /**
     * @brief Iterates all the meta functions of a meta type.
     *
     * Iteratively returns **all** the meta functions of the given type. This
     * means that the meta functions of the base classes will also be returned,
     * if any.
     *
     * @tparam Op Type of the function object to invoke.
     * @param op A valid function object.
     */
    template<typename Op>
    inline void func(Op op) const ENTT_NOEXCEPT {
        internal::iterate<&internal::meta_type_node::func>([op = std::move(op)](auto *node) {
            op(internal::factory(node));
        }, node);
    }

    /**
     * @brief Returns the meta function associated with a given name.
     *
     * Searches recursively among **all** the meta functions of the given type.
     * This means that the meta functions of the base classes will also be
     * inspected, if any.
     *
     * @param str The name to use to search for a meta function.
     * @return The meta function associated with the given name, if any.
     */
    inline meta_func func(const char *str) const ENTT_NOEXCEPT {
        return internal::factory(internal::find_if<&internal::meta_type_node::func>([name = hashed_string{str}](auto *node) {
            return node->name == name;
        }, node));
    }

    /**
     * @brief Creates an instance of the underlying type, if possible.
     *
     * To create a valid instance, the types of the parameters must coincide
     * exactly with those required by the underlying meta constructor.
     * Otherwise, an empty and then invalid container is returned.
     *
     * @tparam Args Types of arguments to use to construct the instance.
     * @param args Parameters to use to construct the instance.
     * @return A meta any containing the new instance, if any.
     */
    template<typename... Args>
    meta_any construct(Args &&... args) const {
        std::array<meta_any, sizeof...(Args)> arguments{{std::forward<Args>(args)...}};
        meta_any any{};

        internal::iterate<&internal::meta_type_node::ctor>([data = arguments.data(), &any](auto *node) -> bool {
            any = node->invoke(data);
            return static_cast<bool>(any);
        }, node);

        return any;
    }

    /**
     * @brief Destroys an instance of the underlying type.
     *
     * It must be possible to cast the instance to the underlying type.
     * Otherwise, invoking the meta destructor results in an undefined behavior.
     *
     * @param handle An opaque pointer to an instance of the underlying type.
     * @return True in case of success, false otherwise.
     */
    inline bool destroy(meta_handle handle) const {
        return node->dtor ? node->dtor->invoke(handle) : node->destroy(handle);
    }

    /**
     * @brief Iterates all the properties assigned to a meta type.
     *
     * Iteratively returns **all** the properties of the given type. This means
     * that the properties of the base classes will also be returned, if any.
     *
     * @tparam Op Type of the function object to invoke.
     * @param op A valid function object.
     */
    template<typename Op>
    inline std::enable_if_t<std::is_invocable_v<Op, meta_prop>, void>
    prop(Op op) const ENTT_NOEXCEPT {
        internal::iterate<&internal::meta_type_node::prop>([op = std::move(op)](auto *node) {
            op(internal::factory(node));
        }, node);
    }

    /**
     * @brief Returns the property associated with a given key.
     *
     * Searches recursively among **all** the properties of the given type. This
     * means that the properties of the base classes will also be inspected, if
     * any.
     *
     * @tparam Key Type of key to use to search for a property.
     * @param key The key to use to search for a property.
     * @return The property associated with the given key, if any.
     */
    template<typename Key>
    inline std::enable_if_t<!std::is_invocable_v<Key, meta_prop>, meta_prop>
    prop(Key &&key) const ENTT_NOEXCEPT {
        return internal::factory(internal::find_if<&internal::meta_type_node::prop>([key = meta_any{std::forward<Key>(key)}](auto *curr) {
            return curr->key() == key;
        }, node));
    }

    /**
     * @brief Returns true if a meta object is valid, false otherwise.
     * @return True if the meta object is valid, false otherwise.
     */
    inline explicit operator bool() const ENTT_NOEXCEPT {
        return node;
    }

    /**
     * @brief Checks if two meta objects refer to the same node.
     * @param other The meta object with which to compare.
     * @return True if the two meta objects refer to the same node, false
     * otherwise.
     */
    inline bool operator==(const meta_type &other) const ENTT_NOEXCEPT {
        return node == other.node;
    }

private:
    const internal::meta_type_node *node;
};


/**
 * @brief Checks if two meta objects refer to the same node.
 * @param lhs A meta object, either valid or not.
 * @param rhs A meta object, either valid or not.
 * @return True if the two meta objects refer to the same node, false otherwise.
 */
inline bool operator!=(const meta_type &lhs, const meta_type &rhs) ENTT_NOEXCEPT {
    return !(lhs == rhs);
}


inline meta_type meta_any::type() const ENTT_NOEXCEPT {
    return internal::factory(node);
}


inline meta_type meta_handle::type() const ENTT_NOEXCEPT {
    return internal::factory(node);
}


inline meta_type meta_base::parent() const ENTT_NOEXCEPT {
    return internal::factory(node->parent);
}


inline meta_type meta_base::type() const ENTT_NOEXCEPT {
    return internal::factory(node->type());
}


inline meta_type meta_conv::parent() const ENTT_NOEXCEPT {
    return internal::factory(node->parent);
}


inline meta_type meta_conv::type() const ENTT_NOEXCEPT {
    return internal::factory(node->type());
}


inline meta_type meta_ctor::parent() const ENTT_NOEXCEPT {
    return internal::factory(node->parent);
}


inline meta_type meta_ctor::arg(size_type index) const ENTT_NOEXCEPT {
    return internal::factory(index < size() ? node->arg(index) : nullptr);
}


inline meta_type meta_dtor::parent() const ENTT_NOEXCEPT {
    return internal::factory(node->parent);
}


inline meta_type meta_data::parent() const ENTT_NOEXCEPT {
    return internal::factory(node->parent);
}


inline meta_type meta_data::type() const ENTT_NOEXCEPT {
    return internal::factory(node->type());
}


inline meta_type meta_func::parent() const ENTT_NOEXCEPT {
    return internal::factory(node->parent);
}


inline meta_type meta_func::ret() const ENTT_NOEXCEPT {
    return internal::factory(node->ret());
}


inline meta_type meta_func::arg(size_type index) const ENTT_NOEXCEPT {
    return internal::factory(index < size() ? node->arg(index) : nullptr);
}


/**
 * @cond TURN_OFF_DOXYGEN
 * Internal details not to be documented.
 */


namespace internal {


template<typename...>
struct meta_function_helper;


template<typename Ret, typename... Args>
struct meta_function_helper<Ret(Args...)> {
    using return_type = Ret;
    using args_type = std::tuple<Args...>;

    template<std::size_t Index>
    using arg_type = std::decay_t<std::tuple_element_t<Index, args_type>>;

    static constexpr auto size = sizeof...(Args);

    inline static auto arg(typename internal::meta_func_node::size_type index) {
        return std::array<meta_type_node *, sizeof...(Args)>{{meta_info<Args>::resolve()...}}[index];
    }
};


template<typename Class, typename Ret, typename... Args, bool Const, bool Static>
struct meta_function_helper<Class, Ret(Args...), std::bool_constant<Const>, std::bool_constant<Static>>: meta_function_helper<Ret(Args...)> {
    using class_type = Class;
    static constexpr auto is_const = Const;
    static constexpr auto is_static = Static;
};


template<typename Ret, typename... Args, typename Class>
constexpr meta_function_helper<Class, Ret(Args...), std::bool_constant<false>, std::bool_constant<false>>
to_meta_function_helper(Ret(Class:: *)(Args...));


template<typename Ret, typename... Args, typename Class>
constexpr meta_function_helper<Class, Ret(Args...), std::bool_constant<true>, std::bool_constant<false>>
to_meta_function_helper(Ret(Class:: *)(Args...) const);


template<typename Ret, typename... Args>
constexpr meta_function_helper<void, Ret(Args...), std::bool_constant<false>, std::bool_constant<true>>
to_meta_function_helper(Ret(*)(Args...));


template<auto Func>
struct meta_function_helper<std::integral_constant<decltype(Func), Func>>: decltype(to_meta_function_helper(Func)) {};


template<typename Type>
inline bool destroy([[maybe_unused]] meta_handle handle) {
    if constexpr(std::is_void_v<Type>) {
        return false;
    } else {
        return handle.type() == internal::factory(meta_info<Type>::resolve())
                ? (static_cast<Type *>(handle.data())->~Type(), true)
                : false;
    }
}


template<typename Type, typename... Args, std::size_t... Indexes>
inline meta_any construct(meta_any * const args, std::index_sequence<Indexes...>) {
    std::array<bool, sizeof...(Args)> can_cast{{(args+Indexes)->can_cast<std::decay_t<Args>>()...}};
    std::array<bool, sizeof...(Args)> can_convert{{(std::get<Indexes>(can_cast) ? false : (args+Indexes)->can_convert<std::decay_t<Args>>())...}};
    meta_any any{};

    if(((std::get<Indexes>(can_cast) || std::get<Indexes>(can_convert)) && ...)) {
        ((std::get<Indexes>(can_convert) ? void((args+Indexes)->convert<std::decay_t<Args>>()) : void()), ...);
        any = Type{(args+Indexes)->cast<std::decay_t<Args>>()...};
    }

    return any;
}


template<bool Const, typename Type, auto Data>
bool setter([[maybe_unused]] meta_handle handle, [[maybe_unused]] meta_any &any) {
    if constexpr(Const) {
        return false;
    } else if constexpr(std::is_member_object_pointer_v<decltype(Data)>) {
        using data_type = std::decay_t<decltype(std::declval<Type>().*Data)>;
        static_assert(std::is_invocable_v<decltype(Data), Type>);
        const bool accepted = any.can_cast<data_type>() || any.convert<data_type>();
        auto *clazz = handle.try_cast<Type>();

        if(accepted && clazz) {
            clazz->*Data = any.cast<data_type>();
        }

        return accepted;
    } else {
        using data_type = std::decay_t<decltype(*Data)>;
        const bool accepted = any.can_cast<data_type>() || any.convert<data_type>();

        if(accepted) {
            *Data = any.cast<data_type>();
        }

        return accepted;
    }
}


template<typename Type, auto Data>
inline meta_any getter([[maybe_unused]] meta_handle handle) {
    if constexpr(std::is_member_object_pointer_v<decltype(Data)>) {
        static_assert(std::is_invocable_v<decltype(Data), Type>);
        auto *clazz = handle.try_cast<Type>();
        return clazz ? meta_any{clazz->*Data} : meta_any{};
    } else {
        return meta_any{*Data};
    }
}


template<typename Type, auto Func, std::size_t... Indexes>
std::enable_if_t<std::is_function_v<std::remove_pointer_t<decltype(Func)>>, meta_any>
invoke(const meta_handle &, meta_any *args, std::index_sequence<Indexes...>) {
    using helper_type = meta_function_helper<std::integral_constant<decltype(Func), Func>>;
    meta_any any{};

    if((((args+Indexes)->can_cast<typename helper_type::template arg_type<Indexes>>()
            || (args+Indexes)->convert<typename helper_type::template arg_type<Indexes>>()) && ...))
    {
        if constexpr(std::is_void_v<typename helper_type::return_type>) {
            (*Func)((args+Indexes)->cast<typename helper_type::template arg_type<Indexes>>()...);
        } else {
            any = meta_any{(*Func)((args+Indexes)->cast<typename helper_type::template arg_type<Indexes>>()...)};
        }
    }

    return any;
}


template<typename Type, auto Member, std::size_t... Indexes>
std::enable_if_t<std::is_member_function_pointer_v<decltype(Member)>, meta_any>
invoke(meta_handle &handle, meta_any *args, std::index_sequence<Indexes...>) {
    using helper_type = meta_function_helper<std::integral_constant<decltype(Member), Member>>;
    static_assert(std::is_base_of_v<typename helper_type::class_type, Type>);
    auto *clazz = handle.try_cast<Type>();
    meta_any any{};

    if(clazz && (((args+Indexes)->can_cast<typename helper_type::template arg_type<Indexes>>()
                  || (args+Indexes)->convert<typename helper_type::template arg_type<Indexes>>()) && ...))
    {
        if constexpr(std::is_void_v<typename helper_type::return_type>) {
            (clazz->*Member)((args+Indexes)->cast<typename helper_type::template arg_type<Indexes>>()...);
        } else {
            any = meta_any{(clazz->*Member)((args+Indexes)->cast<typename helper_type::template arg_type<Indexes>>()...)};
        }
    }

    return any;
}


template<typename Type>
meta_type_node * meta_node<Type>::resolve() ENTT_NOEXCEPT {
    static_assert((std::is_scalar_v<Type> || std::is_class_v<Type> || std::is_void_v<Type>) && !std::is_const_v<Type>);

    if(!type) {
        static meta_type_node node{
            {},
            nullptr,
            nullptr,
            std::is_void_v<Type>,
            std::is_enum_v<Type>,
            std::is_class_v<Type>,
            std::is_pointer_v<Type>,
            std::is_pointer_v<Type> && std::is_function_v<std::remove_pointer_t<Type>>,
            std::is_member_object_pointer_v<Type>,
            std::is_member_function_pointer_v<Type>,
            std::is_member_pointer_v<Type>,
            std::is_arithmetic_v<Type>,
            std::is_compound_v<Type>,
            &destroy<Type>
        };

        type = &node;
    }

    return type;
}


}


/**
 * Internal details not to be documented.
 * @endcond TURN_OFF_DOXYGEN
 */


}


#endif // ENTT_META_META_HPP
