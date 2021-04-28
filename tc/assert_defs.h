
// think-cell public library
//
// Copyright (C) 2016-2021 think-cell Software GmbH
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#ifdef TC_PRIVATE
	#include "Library/ErrorReporting/assert_fwd.h"
#else
	#include "fundamental.h"
	#include <type_traits>
	#include <cstdlib>
	#include <cassert>

	#define _CHECKS
	#ifndef IF_TC_CHECKS
		#define IF_TC_CHECKS(...) __VA_ARGS__
	#endif
	#ifndef IF_TC_DEBUG
		#ifdef _DEBUG
			#define IF_TC_DEBUG(...) __VA_ARGS__
		#else
			#define IF_TC_DEBUG(...)
		#endif
	#endif
	#ifndef _ASSERT
		#include <cassert>
		#define _ASSERT(...) assert((__VA_ARGS__))
	#endif
	#ifndef _ASSERTE
		#define _ASSERTE(...) (static_cast<void>(0))
	#endif
	#ifndef _ASSERTDEBUG
		#define _ASSERTDEBUG(...) IF_TC_DEBUG(_ASSERT((__VA_ARGS__)))
	#endif
	#ifndef _ASSERTFALSE
		#define _ASSERTFALSE _ASSERT(false)
	#endif
	#ifndef _ASSERTNOTIFY
		#define _ASSERTNOTIFY(...) _ASSERT((__VA_ARGS__))
	#endif
	#ifndef _ASSERTNOTIFYFALSE
		#define _ASSERTNOTIFYFALSE _ASSERTFALSE
	#endif
	#ifndef _ASSERTENOTIFY
		#define _ASSERTENOTIFY(...) _ASSERTE((__VA_ARGS__))
	#endif
	#ifndef _ASSERTEQUAL
		#define _ASSERTEQUAL(a, b) assert((a)==(b))
	#endif
	#ifndef _ASSERTEQUALDEBUG
		#define _ASSERTEQUALDEBUG(a, b) IF_TC_DEBUG(_ASSERTEQUAL(a, b))
	#endif
	#ifndef _ASSERTANYOF
		#include <boost/preprocessor/seq/enum.hpp>
		#define _ASSERTANYOFDEBUG(expr, values) [](auto const& e, auto const&... val) noexcept { assert( ((e == val) || ...) ); }(expr, BOOST_PP_SEQ_ENUM(TC_FWD(values)))
	#endif
	#ifndef _ASSERTANYOFDEBUG
		#define _ASSERTANYOFDEBUG(expr, values) IF_TC_DEBUG(_ASSERTANYOF(expr, values))
	#endif
	#ifndef _ASSERTINITIALIZED
		#define _ASSERTINITIALIZED( expr ) static_cast<void>(expr)
	#endif
	#ifndef _ASSERTPRINT
		#define _ASSERTPRINT( cond, ... ) assert( cond )
	#endif
	#ifndef STATICASSERTEQUAL
		#include <boost/preprocessor/facilities/empty.hpp>
		#include <boost/preprocessor/facilities/overload.hpp>
		#define STATICASSERTEQUAL_2(n1, n2) static_assert((n1) == (n2));
		#define STATICASSERTEQUAL_3(n1, n2, strMessage) static_assert((n1) == (n2), strMessage);
		#define STATICASSERTEQUAL(...) BOOST_PP_CAT(BOOST_PP_OVERLOAD(STATICASSERTEQUAL_, __VA_ARGS__)(__VA_ARGS__),BOOST_PP_EMPTY())
	#endif
	#ifndef STATICASSERTSAME
		#include <boost/preprocessor/punctuation/remove_parens.hpp>
		#define STATICASSERTSAME(t1, t2, ...) static_assert(std::is_same<BOOST_PP_REMOVE_PARENS(t1), BOOST_PP_REMOVE_PARENS(t2)>::value, "STATICASSERTSAME failed: " __VA_ARGS__)
	#endif
	#ifndef VERIFYEQUAL
		#include <utility>
		namespace ErrorHandling {
			template <typename Expr, typename Const>
			Expr&& VerifyEqual(Expr&& expr, Const const& c) {
				_ASSERTEQUAL(expr, c);
				return std::forward<Expr>(expr);
			}
		}
		#define VERIFYEQUAL( expr, constant ) ErrorHandling::VerifyEqual(expr, constant)
	#endif
	#ifndef VERIFYEQUALNOPRINT
		#define VERIFYEQUALNOPRINT VERIFYEQUAL
	#endif
	#ifndef VERIFY
		#include <utility>
		namespace ErrorHandling {
			template <typename Expr>
			Expr&& Verify(Expr&& expr) {
				_ASSERT(expr);
				return std::forward<Expr>(expr);
			}
		}
		#define VERIFY( expr ) ErrorHandling::Verify(expr)
	#endif
	#ifndef VERIFYPRED
		#include <utility>
		namespace ErrorHandling {
			template <typename Expr, typename Pred>
			Expr&& VerifyPred(Expr&& expr, Pred pred) {
				_ASSERT(pred(expr));
				return std::forward<Expr>(expr);
			}
		}
		#define VERIFYPRED( expr, ... ) ErrorHandling::VerifyPred(expr, [&](auto const& _) { return (__VA_ARGS__); })
	#endif
	#ifndef VERIFYINITIALIZED
		#define VERIFYINITIALIZED( expr ) (expr)
	#endif
	#ifndef VERIFYNOTIFYEQUAL
		#define VERIFYNOTIFYEQUAL VERIFYEQUAL
	#endif
	#ifndef VERIFYNOTIFY
		#define VERIFYNOTIFY VERIFY
	#endif
	#ifndef VERIFYNOTIFYPRED
		#define VERIFYNOTIFYPRED VERIFYPRED
	#endif
	#ifndef NOBADALLOC
		#define NOBADALLOC( expr ) (expr)
	#endif
	#ifndef NOBADALLOC_LAMBDA
		#define NOBADALLOC_LAMBDA( expr ) (expr)
	#endif
	#ifndef NOEXCEPT
		#define NOEXCEPT( ... ) \
			[&]() noexcept -> decltype(auto) { \
				return (__VA_ARGS__); \
			}()
	#endif
	#ifndef NOEXCEPT_NO_LAMBDA
		#define NOEXCEPT_NO_LAMBDA( ... ) (__VA_ARGS__)
	#endif

	#define switch_no_default(...) \
	switch( auto const& /*lifetime extended until end of switch block*/ __switch=(__VA_ARGS__) ) \
	default: \
		if ( _ASSERTFALSE, false ) {std::abort(); /*never executed, but the compiler might complain about this code path not returning a value*/} \
		else

	namespace tc {
		template<typename T, std::enable_if_t<std::is_void<T>::value>* =nullptr >
		T construct_default_or_terminate() noexcept {}

		template<typename T, std::enable_if_t<std::is_default_constructible<T>::value>* =nullptr >
		T construct_default_or_terminate() noexcept {
			try {
				return T();
			} catch ( ... ) {
				std::abort(); // same behavior as violated noexcept
			}
		}

		template<typename T, std::enable_if_t<!std::is_void<T>::value && !std::is_default_constructible<T>::value>* =nullptr>
		T construct_default_or_terminate() noexcept {
			std::abort(); // same behavior as violated noexcept
		}
	}

	namespace tc {
		using char16 = std::conditional_t<2==sizeof(wchar_t), wchar_t, char16_t>;
	}

#endif
