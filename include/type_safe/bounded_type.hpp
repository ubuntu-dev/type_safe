// Copyright (C) 2016 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#ifndef TYPE_SAFE_BOUNDED_TYPE_HPP_INCLUDED
#define TYPE_SAFE_BOUNDED_TYPE_HPP_INCLUDED

#include <type_traits>

#include <type_safe/constrained_type.hpp>

namespace type_safe
{
    namespace constraints
    {

        struct dynamic_bound { };

        namespace detail
        {
            // Base to enable empty base optimization when BoundConstant is not dynamic_bound.
            // Neccessary when T is not a class.
            template <class T>
            struct Wrapper
            {
                T value;
            };

            template <class BoundConstant>
            struct is_dynamic : std::is_same<BoundConstant,dynamic_bound>
            {
            };

            template <class T, class BoundConstant>
            using Base = typename std::conditional<is_dynamic<BoundConstant>::value,
                                                   Wrapper<T>,
                                                   BoundConstant>::type;
        } // detail namespace

#define TYPE_SAFE_DETAIL_MAKE(Name, Op)                                                            \
    template <typename T, typename BoundConstant = dynamic_bound>                                  \
    class Name : detail::Base<T, BoundConstant>                                                    \
    {                                                                                              \
        using Base = detail::Base<T, BoundConstant>;                                               \
                                                                                                   \
        static constexpr bool is_dynamic = detail::is_dynamic<BoundConstant>::value;               \
    public:                                                                                        \
        explicit Name()                                                                            \
        {                                                                                          \
            static_assert(!is_dynamic,"constructor requires static bound");                        \
        }                                                                                          \
                                                                                                   \
        explicit Name(const T& bound) : Base{bound}                                                \
        {                                                                                          \
            static_assert(is_dynamic,"constructor requires dynamic bound");                        \
        }                                                                                          \
                                                                                                   \
        explicit Name(T&& bound) noexcept(std::is_nothrow_move_constructible<T>::value)            \
        : Base{std::move(bound)}                                                                   \
        {                                                                                          \
            static_assert(is_dynamic,"constructor requires dynamic bound");                        \
        }                                                                                          \
                                                                                                   \
        template <typename U>                                                                      \
        bool operator()(const U& u) const                                                          \
        {                                                                                          \
            return u Op get_bound();                                                               \
        }                                                                                          \
                                                                                                   \
        const T& get_bound() const noexcept                                                        \
        {                                                                                          \
            return Base::value;                                                                    \
        }                                                                                          \
    };

        /// A `Constraint` for the [type_safe::constrained_type<T, Constraint, Verifier>]().
        /// A value is valid if it is less than some given value.
        TYPE_SAFE_DETAIL_MAKE(less, <)

        /// A `Constraint` for the [type_safe::constrained_type<T, Constraint, Verifier>]().
        /// A value is valid if it is less than or equal to some given value.
        TYPE_SAFE_DETAIL_MAKE(less_equal, <=)

        /// A `Constraint` for the [type_safe::constrained_type<T, Constraint, Verifier>]().
        /// A value is valid if it is greater than some given value.
        TYPE_SAFE_DETAIL_MAKE(greater, >)

        /// A `Constraint` for the [type_safe::constrained_type<T, Constraint, Verifier>]().
        /// A value is valid if it is greater than or equal to some given value.
        TYPE_SAFE_DETAIL_MAKE(greater_equal, >=)

#undef TYPE_SAFE_DETAIL_MAKE

        namespace detail
        {
            // checks that that the value is less than the upper bound
            template <bool Inclusive, typename T, typename BoundConstant>
            using upper_bound_t =
                typename std::conditional<Inclusive, less_equal<T, BoundConstant>,
                                                     less<T, BoundConstant>>::type;

            // checks that the value is greater than the lower bound
            template <bool Inclusive, typename T, typename BoundConstant>
            using lower_bound_t =
                typename std::conditional<Inclusive, greater_equal<T, BoundConstant>,
                                                     greater<T, BoundConstant>>::type;
        } // namespace detail

        constexpr bool open   = false;
        constexpr bool closed = true;

        /// A `Constraint` for the [type_safe::constrained_type<T, Constraint, Verifier>]().
        /// A value is valid if it is between two given bounds,
        /// `LowerInclusive`/`UpperInclusive` control whether the lower/upper bound itself is valid too.
        /// `LowerConstant`/`UpperConstant` control whether the lower/upper bound is specified statically or dinamically.
        /// When they are `dynamic_bound`, the bounds are specified at run-time. Otherwise, they must match
        /// the `boost::hana::Constant` concept, in which case their nested `value` is the bound.
        template <typename T, bool LowerInclusive, bool UpperInclusive,
                  typename LowerConstant = dynamic_bound,
                  typename UpperConstant = dynamic_bound>
        class bounded : detail::lower_bound_t<LowerInclusive, T, LowerConstant>,
                        detail::upper_bound_t<UpperInclusive, T, UpperConstant>
        {
            static constexpr bool lower_is_dynamic = detail::is_dynamic<LowerConstant>::value;
            static constexpr bool upper_is_dynamic = detail::is_dynamic<UpperConstant>::value;

            static_assert(lower_is_dynamic==upper_is_dynamic,
                          "mixed static and dynamic bounds is unsupported");

            using Lower = detail::lower_bound_t<LowerInclusive, T, LowerConstant>;
            using Upper = detail::upper_bound_t<UpperInclusive, T, UpperConstant>;

            const Lower& lower() const noexcept { return *this; }
            const Upper& upper() const noexcept { return *this; }

            template <typename U>
            using decay_same = std::is_same<typename std::decay<U>::type, T>;

        public:
            template <typename LC = LowerConstant, typename UC = UpperConstant,
                      typename = typename std::enable_if<
                                    decay_same<typename LC::value_type>::value>::type,
                      typename = typename std::enable_if<
                                    decay_same<typename UC::value_type>::value>::type>
            bounded()
            {
                static_assert(!lower_is_dynamic && !upper_is_dynamic,
                              "constructor requires static bounds");
            }

            template <typename U1, typename U2,
                      typename = typename std::enable_if<decay_same<U1>::value>::type,
                      typename = typename std::enable_if<decay_same<U2>::value>::type>
            bounded(U1&& lower, U2&& upper)
            : Lower(std::forward<U1>(lower)), Upper(std::forward<U2>(upper))
            {
                static_assert(upper_is_dynamic && lower_is_dynamic,
                              "constructor requires dynamic bounds");
            }

            template <typename U>
            bool operator()(const U& u) const
            {
                return lower()(u) && upper()(u);
            }

            const T& get_lower_bound() const noexcept
            {
                return lower().get_bound();
            }

            const T& get_upper_bound() const noexcept
            {
                return upper().get_bound();
            }
        };

        /// A `Constraint` for the [type_safe::constrained_type<T, Constraint, Verifier>]().
        /// A value is valid if it is between two given bounds but not the bounds themselves.
        template <typename T>
        using open_interval = bounded<T, open, open>;

        /// A `Constraint` for the [type_safe::constrained_type<T, Constraint, Verifier>]().
        /// A value is valid if it is between two given bounds or the bounds themselves.
        template <typename T>
        using closed_interval = bounded<T, closed, closed>;
    } // namespace constraints

    /// \exclude
    namespace detail
    {
        template <typename T, typename U1, typename U2>
        struct bounded_value_type_impl
        {
            static_assert(!std::is_same<T, U1>::value && !std::is_same<U1, U2>::value,
                          "make_bounded() called with mismatching types");
        };

        template <typename T>
        struct bounded_value_type_impl<T, T, T>
        {
            using type = T;
        };

        template <typename T, typename U1, typename U2>
        using bounded_value_type =
            typename bounded_value_type_impl<typename std::decay<T>::type,
                                             typename std::decay<U1>::type,
                                             typename std::decay<U2>::type>::type;
    } // namespace detail

    /// An alias for [type_safe::constrained_type<T, Constraint, Verifier>]() that uses [type_safe::constraints::bounded<T, LowerInclusive, UpperInclusive>]() as its `Constraint`.
    /// \notes This is some type where the values must be in a certain interval.
    template <typename T, bool LowerInclusive, bool UpperInclusive,
              typename LowerConstant = constraints::dynamic_bound,
              typename UpperConstant = constraints::dynamic_bound>
    using bounded_type =
        constrained_type<T, constraints::bounded<T, LowerInclusive, UpperInclusive,
                                                    LowerConstant, UpperConstant>>;

    /// \returns A [type_safe::bounded_type<T, LowerInclusive, UpperInclusive>]() with the given `value` and lower and upper bounds,
    /// where those bounds are valid values as well.
    template <typename T, typename U1, typename U2>
    auto make_bounded(T&& value, U1&& lower, U2&& upper)
        -> bounded_type<detail::bounded_value_type<T, U1, U2>, true, true>
    {
        using value_type = detail::bounded_value_type<T, U1, U2>;
        return bounded_type<value_type, true,
                            true>(std::forward<T>(value),
                                  constraints::closed_interval<value_type>(std::forward<U1>(lower),
                                                                           std::forward<U2>(
                                                                               upper)));
    }

    /// \returns A [type_safe::bounded_type<T, LowerInclusive, UpperInclusive>]() with the given `value` and lower and upper bounds,
    /// where those bounds are not valid values.
    template <typename T, typename U1, typename U2>
    auto make_bounded_exclusive(T&& value, U1&& lower, U2&& upper)
        -> bounded_type<detail::bounded_value_type<T, U1, U2>, false, false>
    {
        using value_type = detail::bounded_value_type<T, U1, U2>;
        return bounded_type<value_type, false,
                            false>(std::forward<T>(value),
                                   constraints::open_interval<value_type>(std::forward<U1>(lower),
                                                                          std::forward<U2>(upper)));
    }

    /// \effects Changes `val` so that it is in the interval.
    /// If it is not in the interval, assigns the bound that is closer to the value.
    template <typename T, typename U>
    void clamp(const constraints::closed_interval<T>& interval, U& val)
    {
        if (val < interval.get_lower_bound())
            val = static_cast<U>(interval.get_lower_bound());
        else if (val > interval.get_upper_bound())
            val = static_cast<U>(interval.get_upper_bound());
    }

    /// A `Verifier` for [type_safe::constrained_type<T, Constraint, Verifier]() that clamps the value to make it valid.
    /// It must be used together with [type_safe::constraints::less_equal<T>](), [type_safe::constraints::greater_equal<T>]() or [type_safe::constraints::closed_interval<T>]().
    struct clamping_verifier
    {
        /// \effects If `val` is greater than the bound of `p`,
        /// assigns the bound to `val`.
        /// Otherwise does nothing.
        template <typename Value, typename T>
        static void verify(Value& val, const constraints::less_equal<T>& p)
        {
            if (!p(val))
                val = static_cast<Value>(p.get_bound());
        }

        /// \effects If `val` is less than the bound of `p`,
        /// assigns the bound to `val`.
        /// Otherwise does nothing.
        template <typename Value, typename T>
        static void verify(Value& val, const constraints::greater_equal<T>& p)
        {
            if (!p(val))
                val = static_cast<Value>(p.get_bound());
        }

        /// \effects Same as `clamp(interval, val)`.
        template <typename Value, typename T>
        static void verify(Value& val, const constraints::closed_interval<T>& interval)
        {
            clamp(interval, val);
        }
    };

    /// An alias for [type_safe::constrained_type<T, Constraint, Verifier>]() that uses [type_safe::constraints::closed_interval<T>]() as its `Constraint`
    /// and [type_safe::clamping_verifier]() as its `Verifier`.
    /// \notes This is some type where the values are always clamped so that they are in a certain interval.
    template <typename T>
    using clamped_type = constrained_type<T, constraints::closed_interval<T>, clamping_verifier>;

    /// \returns A [type_safe::clamped_type<T>]() with the given `value` and lower and upper bounds.
    template <typename T, typename U1, typename U2>
    auto make_clamped(T&& value, U1&& lower, U2&& upper)
        -> clamped_type<detail::bounded_value_type<T, U1, U2>>
    {
        using value_type = detail::bounded_value_type<T, U1, U2>;
        return clamped_type<value_type>(std::forward<T>(value),
                                        constraints::closed_interval<value_type>(std::forward<U1>(
                                                                                     lower),
                                                                                 std::forward<U2>(
                                                                                     upper)));
    }
} // namespace type_safe

#endif // TYPE_SAFE_BOUNDED_TYPE_HPP_INCLUDED
