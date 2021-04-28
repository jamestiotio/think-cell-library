
// think-cell public library
//
// Copyright (C) 2016-2021 think-cell Software GmbH
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once
#include "type_traits.h"
#include "assert_defs.h"

#define modified(obj, ...) ([&]() MAYTHROW { auto _=tc::decay_copy(obj); {__VA_ARGS__;} return _; }())

namespace tc {
	namespace no_adl {
		template< typename T >
		struct auto_cref_impl final {
			STATICASSERTSAME(std::remove_cv_t<T>,tc::decay_t<T>);
			using type=std::remove_cv_t<T> const;
		};

		template< typename T >
		struct auto_cref_impl< T& > final {
			using type=T const&;
		};

		template< typename T >
		struct auto_cref_impl< T&& > final {
			STATICASSERTSAME(std::remove_cv_t<T>,tc::decay_t<T>);
			using type=std::remove_cv_t<T> const;
		};

		template< typename T >
		struct auto_cref_return_impl final {
			STATICASSERTSAME(std::remove_cv_t<T>,tc::decay_t<T>);
			using type=std::remove_cv_t<T>;
		};

		template< typename T >
		struct auto_cref_return_impl< T& > final {
			using type=T const&;
		};

		template< typename T >
		struct auto_cref_return_impl< T&& > final {
			STATICASSERTSAME(std::remove_cv_t<T>,tc::decay_t<T>);
			using type=std::remove_cv_t<T>;
		};
		
		// auto_ptr should be used in a modifying context, i.e., represent a mutable reference.
		// If it is built from a value we assume pointer semantics and hold the const value.
		template< typename T >
		struct auto_ptr_impl final {
			STATICASSERTSAME(std::remove_cv_t<T>,tc::decay_t<T>);
			static_assert(std::is_pointer<T>::value); // or pointer-like
			using type=std::remove_cv_t<T> const;
		};

		template< typename T >
		struct auto_ptr_impl< T& > final {
			using type=T&;
		};

		template< typename T >
		struct auto_ptr_impl< T&& > final {
			STATICASSERTSAME(std::remove_cv_t<T>,tc::decay_t<T>);
			static_assert(std::is_pointer<T>::value); // or pointer-like
			using type=std::remove_cv_t<T> const;
		};
	}
};

#define auto_cref( var, ... ) \
	typename tc::no_adl::auto_cref_impl<decltype((__VA_ARGS__))>::type var = __VA_ARGS__
#define auto_cref_return( var, ... ) \
	typename tc::no_adl::auto_cref_return_impl<decltype((__VA_ARGS__))>::type var = __VA_ARGS__
#define auto_ptr( var, ... ) \
	typename tc::no_adl::auto_ptr_impl<decltype((__VA_ARGS__))>::type var = __VA_ARGS__
