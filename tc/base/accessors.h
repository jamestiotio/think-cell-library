
// think-cell public library
//
// Copyright (C) 2016-2022 think-cell Software GmbH
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include "assert_defs.h"
#include "generic_macros.h"
#include "type_traits_fwd.h"

#include <boost/preprocessor/comparison/greater.hpp>

namespace tc {
	// customization point accessor_return_type
	namespace no_adl {
		template<typename T>
		struct accessor_return_type final { 
			static_assert(tc::decayed<T>);
			using const_ref_type = T;
			using ref_ref_type = T;
			using const_ref_ref_type = T;
		};

		template<typename T> requires
			std::is_union<std::remove_reference_t<T>>::value ||
			std::is_class<std::remove_reference_t<T>>::value ||
			std::is_array<std::remove_reference_t<T>>::value
		struct accessor_return_type<T> final {
			static_assert(std::is_array<T>::value || tc::decayed<T>);
			using const_ref_type = T const&;
			using ref_ref_type = T&&;
			using const_ref_ref_type = T const&&;
		};
	}
}

#define DEFINE_MEMBER_WITHOUT_INIT(type, name) \
		type name;
#define DEFINE_MEMBER_WITH_INIT(type, name, ...) \
		type name{__VA_ARGS__};

#define DEFINE_MEMBER_BASE(type, ...) \
	TC_EXPAND(TC_EXPAND(BOOST_PP_IF(BOOST_PP_GREATER(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), 1), DEFINE_MEMBER_WITH_INIT, DEFINE_MEMBER_WITHOUT_INIT))(TC_FWD(type), __VA_ARGS__))

#define DEFINE_ACCESSORS_BASE(type, funcname, invariant, name) \
	public: \
		constexpr typename tc::no_adl::accessor_return_type<type>::const_ref_type funcname() const& noexcept { invariant(name); return name; } \
		\
		template<ENABLE_SFINAE> requires /*dummy constraint making this function preferred in overload resolution over the deleted function below*/true \
		constexpr typename tc::no_adl::accessor_return_type<SFINAE_TYPE(type)>::ref_ref_type funcname() && noexcept { invariant(name); return tc_move(name); } \
		\
		template<ENABLE_SFINAE> /* needed to stay behind non-deleted function in overload resolution */ \
		constexpr type&& funcname() && noexcept = delete; /* Visual Studio gives improper error message if it returns a dummy type */ \
		\
		template<ENABLE_SFINAE> requires true \
		constexpr typename tc::no_adl::accessor_return_type<SFINAE_TYPE(type)>::const_ref_ref_type funcname() const&& noexcept { invariant(name); return std::move(name); } \
		\
		template<ENABLE_SFINAE> \
		constexpr type const&& funcname() const&& noexcept = delete; /* Visual Studio gives improper error message if it returns a dummy type */

#define DEFINE_MEMBER_INVARIANT(...) ([&](auto const& _) constexpr noexcept { \
	_ASSERTINITIALIZED(_); \
	__VA_ARGS__; \
})

#define DEFINE_MEMBER_AND_NAMED_ACCESSORS_INVARIANT(type, funcname, invariant, ...) /* type, funcname, invariant, name(, value) */ \
	DEFINE_MEMBER_BASE(TC_FWD(type), __VA_ARGS__) \
	DEFINE_ACCESSORS_BASE(TC_FWD(type), funcname, DEFINE_MEMBER_INVARIANT(invariant), BOOST_PP_VARIADIC_ELEM(0, __VA_ARGS__)) \
	private:

#define DEFINE_MEMBER_AND_NAMED_ACCESSORS(type, funcname, ...) /* type, funcname, name(, value) */ \
	DEFINE_MEMBER_AND_NAMED_ACCESSORS_INVARIANT(TC_FWD(type), funcname, BOOST_PP_EMPTY(), __VA_ARGS__)

#define DEFINE_PROTECTED_MEMBER_AND_NAMED_ACCESSORS_INVARIANT(type, funcname, invariant, ...) /* type, funcname, invariant, name(, value) */ \
	DEFINE_MEMBER_BASE(TC_FWD(type), __VA_ARGS__) \
	DEFINE_ACCESSORS_BASE(TC_FWD(type), funcname, DEFINE_MEMBER_INVARIANT(invariant), BOOST_PP_VARIADIC_ELEM(0, __VA_ARGS__)) \
	protected:

#define DEFINE_PROTECTED_MEMBER_AND_NAMED_ACCESSORS(type, funcname, ...) /* type, funcname, name(, value) */ \
	DEFINE_PROTECTED_MEMBER_AND_NAMED_ACCESSORS_INVARIANT(TC_FWD(type), funcname, BOOST_PP_EMPTY(), __VA_ARGS__)

#define DEFINE_MEMBER_AND_ACCESSORS_INVARIANT(type, invariant, ...) /* type, invariant, name(, value) */ \
	DEFINE_MEMBER_BASE(TC_FWD(type), __VA_ARGS__) \
	DEFINE_ACCESSORS_BASE(TC_FWD(type), BOOST_PP_CAT(BOOST_PP_VARIADIC_ELEM(0, __VA_ARGS__), _), DEFINE_MEMBER_INVARIANT(invariant), BOOST_PP_VARIADIC_ELEM(0, __VA_ARGS__)) \
	private:

#define DEFINE_MEMBER_AND_ACCESSORS(type, ...) /* type, name(, value) */ \
	DEFINE_MEMBER_AND_ACCESSORS_INVARIANT(TC_FWD(type), BOOST_PP_EMPTY(), __VA_ARGS__)

#define DEFINE_PROTECTED_MEMBER_AND_ACCESSORS_INVARIANT(type, invariant, ...) /* type, invariant, name(, value) */ \
	DEFINE_MEMBER_BASE(TC_FWD(type), __VA_ARGS__) \
	DEFINE_ACCESSORS_BASE(TC_FWD(type), BOOST_PP_CAT(BOOST_PP_VARIADIC_ELEM(0, __VA_ARGS__), _), DEFINE_MEMBER_INVARIANT(invariant), BOOST_PP_VARIADIC_ELEM(0, __VA_ARGS__)) \
	protected:

#define DEFINE_PROTECTED_MEMBER_AND_ACCESSORS(type, ...) /* type, name(, value) */ \
	DEFINE_PROTECTED_MEMBER_AND_ACCESSORS_INVARIANT(TC_FWD(type), BOOST_PP_EMPTY(), __VA_ARGS__)