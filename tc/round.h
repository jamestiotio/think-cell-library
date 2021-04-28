
// think-cell public library
//
// Copyright (C) 2016-2021 think-cell Software GmbH
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include "explicit_cast.h"
#include "return_decltype.h"
#include "inplace.h"
#include "type_traits.h"
#include "integer.h"
#include "compare.h"
#include <type_traits>
#include <chrono>

namespace tc {
	template<typename TTarget, typename TSource, std::enable_if_t<tc::is_base_of_decayed<TTarget, TSource>::value>* = nullptr>
	[[nodiscard]] constexpr TSource&& reluctant_numeric_cast(TSource&& src) noexcept {
		STATICASSERTSAME(tc::remove_cvref_t<TTarget>, TTarget);
		return std::forward<TSource>(src);
	}

	template<typename TTarget, typename TSource, std::enable_if_t<!tc::is_base_of_decayed<TTarget, TSource>::value>* = nullptr>
	[[nodiscard]] constexpr TTarget reluctant_numeric_cast(TSource&& src) noexcept {
		STATICASSERTSAME(tc::remove_cvref_t<TTarget>, TTarget);
		return tc::explicit_cast<TTarget>(std::forward<TSource>(src));
	}

	//////////////////////////////
	// round

	template< typename T >
	[[nodiscard]] T round( T t ) noexcept { // We cannot make tc::round constexpr at the moment. tc::round uses std::floor which is not constexpr. We have to use reinterpret_cast if we want to implement our own floor(f).
		static_assert( std::is_floating_point<T>::value );
		return std::floor(t+static_cast<T>(.5));
	}

	//////////////////////////////
	// type-safe multiplication and division

	template< typename T1, typename T2, typename Enable=void >
	struct TMultiply final {};

	template< typename T1, typename T2 >
	struct TMultiply<T1, T2, std::enable_if_t< 
		tc::is_actual_integer_like< T1 >::value && 
		tc::is_actual_integer_like< T2 >::value &&
		( std::numeric_limits<T1>::is_signed || std::numeric_limits<T2>::is_signed )
	>> final {
		using type=typename tc::integer< std::numeric_limits<T1>::digits+std::numeric_limits<T2>::digits
			// For 2s complement representation,
			// in case of both operands signed, -(2^31) * -(2^31) = -2^62, which needs 63 bits to be represented.
			// digits is length-1 for signed numbers, so 31+31+1=63, OK!
			// In case of one unsigned and one signed, (2^32-1) * -2^31 = -2^63 + 2^31, which needs 63 bits to be represented.
			// digits is length for unsigned and length-1 for signed numbers, so 32+31=63, OK!
			+( std::numeric_limits<T1>::is_signed && std::numeric_limits<T2>::is_signed ) >::signed_;
	};

	template< typename T1, typename T2 >
	struct TMultiply<T1, T2, std::enable_if_t< 
		tc::is_actual_integer_like< T1 >::value && 
		tc::is_actual_integer_like< T2 >::value &&
		!std::numeric_limits<T1>::is_signed && !std::numeric_limits<T2>::is_signed
	>> final {
		using type=typename tc::integer< std::numeric_limits<T1>::digits+std::numeric_limits<T2>::digits >::unsigned_;
	};

	template< typename T1, typename T2 >
	struct TMultiply<T1, T2, std::enable_if_t< std::is_floating_point< T1 >::value || std::is_floating_point< T2 >::value >> final {
		using type = decltype(std::declval<T1>()*std::declval<T2>());
	};

	template< typename Lhs, typename Rhs >
	[[nodiscard]]
	typename TMultiply<Lhs,Rhs>::type
	constexpr mul( Lhs lhs, Rhs rhs ) noexcept {
		return tc::explicit_cast<typename TMultiply<Lhs,Rhs>::type>(lhs)*tc::explicit_cast<typename TMultiply<Lhs,Rhs>::type>(rhs);
	}

	template<typename T>
	[[nodiscard]] constexpr auto sqr(T const& x)
		return_decltype_noexcept( mul(x,x) )

#ifdef _MSC_VER
	#pragma float_control(precise, on, push) // RT#17630: Disable compiler optimization num/den ---> num*(1/den) resulting in tc::fdiv(1287,100)==12.870000000000001
#endif
	[[nodiscard]] inline constexpr 
#ifdef __clang__
		__attribute__((optnone)) // Disable compiler optimization num/den ---> num*(1/den) resulting in tc::fdiv(49,49)==0.99999999999999989
#endif
	double fdiv(double num, double den) noexcept {
		return num/den;
	}
#ifdef _MSC_VER
	#pragma float_control(pop)
#endif
	
	template< typename Num, typename Den >
	[[nodiscard]] constexpr double fdiv(Num num, Den den) noexcept {
		return tc::fdiv(static_cast<double>(num), static_cast<double>(den));
	}

	struct SRoundFloor final {
		template< typename T, std::enable_if_t<std::is_integral<T>::value>* = nullptr>
		constexpr T operator()( T t ) const& noexcept {
			return t;
		}
		template< typename T, std::enable_if_t<std::is_floating_point<T>::value>* = nullptr>
		T operator()( T t ) const& noexcept {
			return std::floor(t);
		}
		template< typename T >
		static constexpr T PositiveOffset( T t ) noexcept {
			return 0;
		}
		template< typename T >
		static constexpr T NegativeOffset( T t ) noexcept {
			return t-1;
		}
	} inline constexpr roundFLOOR{};

	struct SRoundNearest final {
		template< typename T, std::enable_if_t<std::is_integral<T>::value>* = nullptr>
		constexpr T operator()( T t ) const& noexcept {
			return t;
		}
		template< typename T, std::enable_if_t<std::is_floating_point<T>::value>* = nullptr>
		T operator()( T t ) const& noexcept {
			return tc::round(t);
		}
		template< typename T >
		static constexpr T PositiveOffset( T t ) noexcept {
			return t/2;
		}
		template< typename T >
		static constexpr T NegativeOffset( T t ) noexcept {
			return (t-1)/2;
		}
	} inline constexpr roundNEAREST{};

	struct SRoundAwayFromZero final {
		template< typename T, std::enable_if_t<std::is_integral<T>::value>* = nullptr>
		constexpr T operator()( T t ) const& noexcept {
			return t;
		}
		template< typename T, std::enable_if_t<std::is_floating_point<T>::value>* = nullptr>
		T operator()( T t ) const& noexcept {
			return std::round(t);
		}
		template< typename T >
		static constexpr T PositiveOffset( T t ) noexcept {
			return t/2;
		}
		template< typename T >
		static constexpr T NegativeOffset( T t ) noexcept {
			return t/2;
		}
	} inline constexpr roundAWAYFROMZERO{};

	struct SRoundCeil final {
		template< typename T, std::enable_if_t<std::is_integral<T>::value>* = nullptr>
		constexpr T operator()( T t ) const& noexcept {
			return t;
		}
		template< typename T, std::enable_if_t<std::is_floating_point<T>::value>* = nullptr>
		T operator()( T t ) const& noexcept {
			return std::ceil(t);
		}
		template< typename T >
		static constexpr T PositiveOffset( T t ) noexcept {
			return t-1;
		}
		template< typename T >
		static constexpr T NegativeOffset( T t ) noexcept {
			return 0;
		}
	} inline constexpr roundCEIL{};

	struct SRoundBanker final {
	} inline constexpr roundBANKER{};

	namespace idiv_impl {
		// The standard guarantees that integer division rounds to zero.
		// [expr.mul]/4 (oder 5.6/4) 
		template< typename Num, typename Denom, typename Round >
		constexpr Num idiv( Num num, Denom denom, Round round ) noexcept {
			static_assert( tc::is_actual_integer_like<Num>::value );
			static_assert( tc::is_actual_integer_like< Denom >::value );
			STATICASSERTEQUAL( std::numeric_limits<Num>::is_signed, std::numeric_limits<Denom>::is_signed );
			_ASSERTE( 0<denom );
			return tc::explicit_cast<Num>( ( num<0 ? num-Round::NegativeOffset(denom) : num+Round::PositiveOffset(denom) )/denom );
		}

		template< typename Num, typename Denom >
		constexpr Num idiv( Num num, Denom denom, SRoundBanker ) noexcept {
			static_assert( tc::is_actual_integer_like<Num>::value );
			static_assert( tc::is_actual_integer_like< Denom >::value );
			STATICASSERTEQUAL( std::numeric_limits<Num>::is_signed, std::numeric_limits<Denom>::is_signed );
			_ASSERTE( 0<denom );
			auto result = num / denom;
			auto remainder = num - result * denom;
			if(remainder<0) -tc::inplace(remainder);
			switch_no_default(tc::compare(denom,remainder * 2)) {
				case tc::order::equal:
					if(result % 2 == 0) break;
				case tc::order::less:
					result += num < 0 ? -1 : 1;
					break;
				case tc::order::greater:
					break;
			}
			return tc::explicit_cast<Num>(result);
		}
	}

	template< typename Num, typename Denom, typename Round, std::enable_if_t<
		tc::is_actual_integer_like<Num>::value && tc::is_actual_integer_like< Denom >::value && std::numeric_limits<Num>::is_signed && std::numeric_limits<Denom>::is_signed
	>* = nullptr>
	[[nodiscard]] constexpr Num idiv(Num num, Denom denom, Round round) noexcept {
		if (denom < 0) {
			-tc::inplace(num);
			-tc::inplace(denom);
		}
		return idiv_impl::idiv(num, denom, round);
	}

	template< typename Num, typename Denom, typename Round, std::enable_if_t<
		tc::is_actual_integer_like<Num>::value && tc::is_actual_integer_like< Denom >::value && !std::numeric_limits<Num>::is_signed && !std::numeric_limits<Denom>::is_signed
	>* = nullptr>
	[[nodiscard]] constexpr Num idiv( Num num, Denom denom, Round round ) noexcept {
		return idiv_impl::idiv( num, denom, round );
	}

	template< typename Num, typename Denom, typename Round, std::enable_if_t<
		tc::is_actual_integer_like<Num>::value && tc::is_actual_integer_like< Denom >::value && std::numeric_limits<Num>::is_signed && !std::numeric_limits<Denom>::is_signed
	>* = nullptr>
	[[nodiscard]] constexpr Num idiv( Num num, Denom denom, Round round ) noexcept {
		return idiv/*not idiv_impl::idiv*/( num, tc::signed_cast(denom), round );
	}

	template< typename Num, typename Denom, typename Round, std::enable_if_t<
		tc::is_actual_integer_like<Num>::value && tc::is_actual_integer_like< Denom >::value && !std::numeric_limits<Num>::is_signed && std::numeric_limits<Denom>::is_signed
	>* = nullptr>
	[[nodiscard]] constexpr Num idiv( Num num, Denom denom, Round round ) noexcept {
		return idiv_impl::idiv( num, tc::unsigned_cast(denom), round );
	}

	template< typename Num, typename Denom, typename Round, std::enable_if_t<
		tc::is_actual_integer_like<Num>::value && std::is_floating_point< Denom >::value
	>* = nullptr>
	[[nodiscard]] constexpr Num idiv( Num num, Denom denom, Round round ) noexcept {
		return tc::explicit_cast<Num>(round(num/denom));
	}

	template< typename Num, typename Denom, typename Round >
	[[nodiscard]] std::pair<Num,Denom> idivmod( Num num, Denom denom, Round round ) noexcept {
		Num div=tc::idiv(num,denom,round);
		Denom mod=num-div*denom;
		return std::make_pair(div,mod);
	}

	/////////////////////////////////
	// scale_div

	template< typename Num, typename Denom, typename Round, std::enable_if_t<tc::is_actual_integer< Num >::value>* = nullptr >
	[[nodiscard]] constexpr Num scale_div( Num num, Denom denom, Round round ) noexcept {
		return idiv(num,denom,round);
	}

	template< typename Num, typename Denom, std::enable_if_t<std::is_floating_point< Num >::value>* = nullptr >
	[[nodiscard]] constexpr Num scale_div( Num num, Denom denom, SRoundNearest ) noexcept {
		return tc::explicit_cast<Num>(num/denom);
	}

	template< typename Num, typename Denom >
	[[nodiscard]] constexpr Num scale_div( Num num, Denom denom ) noexcept {
		return tc::scale_div(num,denom,tc::roundNEAREST);
	}

	template< bool bGeneralized, typename T, std::enable_if_t<tc::is_actual_integer< T >::value>* = nullptr >
	[[nodiscard]] constexpr T internal_lower_half(T t) noexcept {
		return tc::scale_div(t, 2, tc::roundFLOOR);
	}

	template< bool bGeneralized, typename T, std::enable_if_t<std::is_floating_point< T >::value>* = nullptr >
	[[nodiscard]] constexpr T internal_lower_half(T t) noexcept {
		return t / 2;
	}

	template< bool bGeneralized, typename Rep, typename Period, std::enable_if_t<bGeneralized>* = nullptr >
	[[nodiscard]] std::chrono::duration<Rep, Period> internal_lower_half(std::chrono::duration<Rep, Period> const& dur) noexcept {
		return std::chrono::duration<Rep, Period>(idiv(dur.count(), 2, tc::roundFLOOR));
	}

	template< typename T >
	[[nodiscard]] constexpr auto lower_half(T&& t) return_decltype_noexcept( tc::internal_lower_half</*bGeneralized*/ false>(std::forward<T>(t)) )

	template< bool bGeneralized, typename T >
	[[nodiscard]] constexpr auto internal_midpoint(T const& begin, T const& end) noexcept {
		return begin + tc::internal_lower_half<bGeneralized>(end - begin);
	}

	template< typename T >
	[[nodiscard]] constexpr auto midpoint(T const& begin, T const& end) return_decltype_noexcept( tc::internal_midpoint</*bGeneralized*/false>(begin, end) )

	/////////////////////////////////
	// scale_mul

	template<typename T, typename Factor, std::enable_if_t<
		tc::is_actual_integer< T >::value && std::is_integral< Factor >::value
	>* = nullptr>
	[[nodiscard]] constexpr T scale_mul(T t, Factor factor, SRoundNearest ) noexcept {
		return tc::explicit_cast<T>(tc::prepare_argument<T,Factor>::prepare(t)*tc::prepare_argument<T,Factor>::prepare(factor));
	}

	template<typename T, typename Factor, typename TRound, std::enable_if_t<
		tc::is_actual_integer< T >::value && std::is_floating_point< Factor >::value
	>* = nullptr>
	[[nodiscard]] constexpr T scale_mul(T t, Factor factor, TRound round) noexcept {
		return tc::explicit_cast<T>(round(t*factor));
	}

	template<typename T, typename Factor, std::enable_if_t<
		std::is_floating_point< T >::value
	>* = nullptr>
	[[nodiscard]] constexpr T scale_mul(T t, Factor factor, SRoundNearest ) noexcept {
		return tc::explicit_cast<T>(t*factor);
	}

	template<typename T, typename Factor>
	[[nodiscard]] constexpr T scale_mul(T t, Factor factor) noexcept {
		return scale_mul(t,factor,tc::roundNEAREST);
	}

	/////////////////////////////////
	// scale_muldiv

	template<typename T, typename Num, typename Den, typename TRound, std::enable_if_t<
		tc::is_actual_integer< T >::value && tc::is_actual_integer< Num >::value && tc::is_actual_integer< Den >::value
	>* = nullptr>
	[[nodiscard]] constexpr T scale_muldiv(T t, Num num, Den den, TRound round) noexcept {
		return tc::reluctant_numeric_cast<T>(idiv(mul(t, num), den, round));
	}

	template<typename T, typename Num, typename Den, typename TRound, std::enable_if_t<
		tc::is_actual_integer< T >::value && tc::is_actual_integer< Num >::value && tc::is_floating_point_like< Den >::value
		>* = nullptr>
	[[nodiscard]] constexpr T scale_muldiv(T t, Num num, Den den, TRound round) noexcept {
		return tc::reluctant_numeric_cast<T>(round(mul(t, num) / den));
	}

	template<typename T, typename Num, typename Den, typename TRound, std::enable_if_t<
		tc::is_actual_integer< T >::value && tc::is_floating_point_like< Num >::value
		>* = nullptr>
	[[nodiscard]] constexpr T scale_muldiv(T t, Num num, Den den, TRound round) noexcept {
		return tc::reluctant_numeric_cast<T>(round(t * tc::fdiv(num, den)));
	}

	template<typename T, typename Num, typename Den, std::enable_if_t<
		tc::is_floating_point_like< T >::value
	>* = nullptr>
	[[nodiscard]] constexpr T scale_muldiv(T t, Num num, Den den, SRoundNearest) noexcept {
		return tc::reluctant_numeric_cast<T>(t * tc::fdiv(num, den));
	}

	template<typename T, typename Num, typename Den>
	[[nodiscard]] constexpr T scale_muldiv(T const& x, Num const& num, Den const& den) noexcept {
		return scale_muldiv(x, num, den, tc::roundNEAREST);
	}

	struct fraction {
		int m_nNum;
		int m_nDen;
	};

	template<typename T>
	[[nodiscard]] constexpr T scale_muldiv(T const& x, tc::fraction const& fracn) noexcept {
		return scale_muldiv(x, fracn.m_nNum, fracn.m_nDen);
	}

	/////////////////////////////////
	// rounding_cast
	template<typename Dst, typename Src, typename TRound,
		std::enable_if_t<
			(tc::is_actual_integer<tc::decay_t<Src>>::value && (tc::is_actual_integer<Dst>::value || tc::is_floating_point_like<Dst>::value))
			|| (tc::is_floating_point_like<tc::decay_t<Src>>::value && tc::is_floating_point_like<Dst>::value)
		>* = nullptr
	>
	[[nodiscard]] decltype(auto) rounding_cast(Src&& x, TRound) noexcept {
		return tc::reluctant_numeric_cast<Dst>(std::forward<Src>(x));
	}

	template<typename Dst, typename Src, typename TRound,
		std::enable_if_t<tc::is_floating_point_like<tc::decay_t<Src>>::value && tc::is_actual_integer<Dst>::value>* = nullptr
	>
	[[nodiscard]] decltype(auto) rounding_cast(Src&& x, TRound round) noexcept {
		return tc::reluctant_numeric_cast<Dst>(round(std::forward<Src>(x)));
	}

	template<typename Dst, typename Src>
	[[nodiscard]] decltype(auto) rounding_cast(Src&& x) noexcept {
		return rounding_cast<Dst>(std::forward<Src>(x), roundNEAREST);
	}

	template<typename Lhs, typename Rhs, typename TRound>
	void assign_rounding_cast(Lhs& lhs, Rhs&& rhs, TRound round) noexcept {
		lhs=tc::rounding_cast<Lhs>(std::forward<Rhs>(rhs), round);
	}

	template<typename Lhs, typename Rhs>
	void assign_rounding_cast(Lhs& lhs, Rhs&& rhs) noexcept {
		lhs=tc::rounding_cast<Lhs>(std::forward<Rhs>(rhs));
	}

	namespace no_adl {
		struct fn_div {
			template<typename Num, typename Denom, typename Quot = decltype(std::declval<Num>() / std::declval<Denom>())>
			[[nodiscard]] constexpr Quot operator()(Num&& num, Denom&& denom) const& noexcept {
				if constexpr( tc::is_actual_integer_like<tc::decay_t<Num>>::value && tc::is_actual_integer_like<tc::decay_t<Denom>>::value ) {
					// TODO static_assert( dependent_false<Num, Denom>::value, "Do not rely on language int/int-behavior, use tc::scale_div " );
					return tc::idiv(tc::explicit_cast<Quot>(std::forward<Num>(num)), std::forward<Denom>(denom), tc::roundNEAREST);
				} else {
					static_assert( std::is_floating_point<tc::decay_t<Num>>::value || std::is_floating_point<tc::decay_t<Denom>>::value || std::is_class<tc::decay_t<Num>>::value || std::is_class<tc::decay_t<Denom>>::value );
					static_assert( !tc::is_instance<tc::size_proxy, tc::decay_t<Num>>::value );
					static_assert( !tc::is_instance<tc::size_proxy, tc::decay_t<Denom>>::value );
					return std::forward<Num>(num) / std::forward<Denom>(denom);
				}
			}
		};

		struct fn_assign_div {
			template<typename Num, typename Denom>
			[[nodiscard]] constexpr Num& operator()(Num& num, Denom&& denom) const& noexcept {
				if constexpr( tc::is_actual_integer_like<tc::decay_t<Num>>::value ) {
					// TODO static_assert( dependent_false<Num, Denom>::value, "Do not rely on language int/int-behavior, use tc::scale_div " );
					num = tc::idiv(num, std::forward<Denom>(denom), tc::roundNEAREST);
				} else {
					static_assert( std::is_floating_point<tc::decay_t<Num>>::value || std::is_class<tc::decay_t<Num>>::value );
					static_assert( !tc::is_instance<tc::size_proxy, tc::decay_t<Num>>::value );
					num /= std::forward<Denom>(denom);
				}
				return num;
			}
		};
	}
	using no_adl::fn_div;
	using no_adl::fn_assign_div;
}
