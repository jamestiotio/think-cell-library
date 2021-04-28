
// think-cell public library
//
// Copyright (C) 2016-2021 think-cell Software GmbH
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

#include "range.h"
#include "container.h" // tc::vector
#include "range.t.h"
#include "join_adaptor.h"
#include "concat_adaptor.h"

#include "sparse_adaptor.h"

namespace {

static_assert( tc::is_range_of<tc::is_char, wchar_t const* const>::value );

//---- Basic ------------------------------------------------------------------------------------------------------------------
UNITTESTDEF( basic ) {
	TEST_init_hack(tc::vector, int, v, {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20});

	auto evenvr = tc::filter(v, [](int const v) noexcept { return v%2==0;});

	TEST_init_hack(tc::vector, int, vexp, {2, 4, 6, 8, 10, 12, 14, 16, 18, 20});
	TEST_RANGE_EQUAL(vexp, evenvr);
}

	template<typename Func>
	struct WrapVoidFunc final {
		static_assert(
			std::is_reference<Func>::value,
			"type must be a reference type"
		);

		WrapVoidFunc(Func func, tc::break_or_continue& breakorcontinue) noexcept :
			m_func(std::move(func)), m_breakorcontinue(breakorcontinue)
		{}

		template<typename Arg>
		void operator()(Arg&& arg) & noexcept {
			if (tc::continue_ == m_breakorcontinue) {
				if constexpr( std::is_same<decltype(std::declval<std::remove_reference_t<Func> >()(std::declval<Arg>())), tc::break_or_continue>::value ) {
					m_breakorcontinue = m_func(std::forward<Arg>(arg));
				} else {
					m_func(std::forward<Arg>(arg));
				}
			}
		}

		private:
			Func m_func;
			tc::break_or_continue& m_breakorcontinue;
	};

	template<typename Rng>
	struct void_range_struct final : std::remove_reference_t<Rng> {

		using base_ = std::remove_reference_t<Rng>;

		template< typename Func>
		tc::break_or_continue operator()(Func&& func) & noexcept {
			if constexpr( std::is_same<decltype(std::declval<base_>()(std::declval<Func>())), tc::break_or_continue>::value ) {
				return base_::operator()(std::forward<Func>(func));
			} else {
				tc::break_or_continue breakorcontinue = tc::continue_;
				base_::operator()(WrapVoidFunc<Func&&>(std::forward<Func>(func), breakorcontinue));
				return breakorcontinue;
			}
		}

		template< typename Func>
		tc::break_or_continue operator()(Func&& func) const& noexcept {
			if constexpr( std::is_same<decltype(std::declval<base_>()(std::declval<Func>())), tc::break_or_continue>::value ) {
				return base_::operator()(std::forward<Func>(func));
			} else {
				tc::break_or_continue breakorcontinue = tc::continue_;
				base_::operator()(WrapVoidFunc<Func&&>(std::forward<Func>(func), breakorcontinue));
				return breakorcontinue;
			}
		}
	};

	template<typename Rng>
	auto void_range(Rng&& rng) return_decltype_xvalue_by_ref_MAYTHROW(
		tc::derived_or_base_cast<void_range_struct<Rng&&>>(std::forward<Rng>(rng))
	)


//---- Generator Range --------------------------------------------------------------------------------------------------------
namespace {
	struct generator_range {
		template< typename Func >
		void operator()(Func func) const& noexcept {
			for(int i=0;i<50;++i) {
				func(i);
			}
		}
	};
}

UNITTESTDEF( generator_range ) {
   TEST_init_hack(tc::vector, int, vexp, {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48});

   TEST_RANGE_EQUAL(vexp, tc::filter( void_range(generator_range()), [](int i) noexcept { return i%2==0; } ));
   TEST_RANGE_EQUAL(tc::filter( void_range(generator_range()), [](int i) noexcept { return i%2==0; } ), vexp);
}

//---- Generator Range (with break) -------------------------------------------------------------------------------------------
namespace {
	struct generator_range_break final {
		template< typename Func >
		tc::break_or_continue operator()(Func func) {
			for(int i=0;i<5000;++i) {
				if (func(i)==tc::break_) { return tc::break_; }
			}
			return tc::continue_;
		}
	};
}

//---- N3752 filters examples  ------------------------------------------------------------------------------------------------
UNITTESTDEF( N3752 ) {
   TEST_init_hack(tc::vector, int, v, {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20});

   auto r =  tc::filter( tc::filter( tc::filter(
                                v,
                                [](int i) noexcept { return i%2!=0; } ),
                                [](int i) noexcept { return i%3!=0; } ),
                                [](int i) noexcept { return i%5!=0; } );

   TEST_init_hack(tc::vector, int, vexp, {1, 7, 11, 13, 17, 19});
   TEST_RANGE_EQUAL(vexp, r);

   auto ir = tc::make_iterator_range(std::begin(r), std::end(r));    // you shouldn't do this in real code! 
   TEST_RANGE_EQUAL(vexp, ir);

   auto bir = boost::make_iterator_range(tc::begin(r), tc::end(r));    // you shouldn't do this in real code! 
   TEST_RANGE_EQUAL(vexp, bir);
}

UNITTESTDEF( zero_termination ) {
	// only char is treated as zero-terminated character array.
	// signed/unsigned char is treated as a regular array
	{
		char const ach[]={ 0x20, 0 };
		_ASSERTEQUAL( tc::size(ach), 1 );
		char const* pch=ach;
		_ASSERTEQUAL( tc::size(pch), 1 );
	}
	{
		signed char const ach[]={ 0x20, 0 };
		_ASSERTEQUAL( tc::size(ach), 2 );
		// signed char const* pch=ach;
		// _ASSERTEQUAL( tc::size(pch), 2 ); // correctly refuses to compile
	}
	{
		unsigned char const ach[]={ 0x20, 0 };
		_ASSERTEQUAL( tc::size(ach), 2 );
		// unsigned char const* pch=ach;
		// _ASSERTEQUAL( tc::size(pch), 2 ); // correctly refuses to compile
	}
}

UNITTESTDEF( ensure_index_range_on_chars ) {
	static_assert( tc::is_range_with_iterators<char*>::value );
	static_assert( tc::is_range_with_iterators<char const*>::value );
	static_assert( tc::is_range_with_iterators<char* &>::value );
	static_assert( tc::is_range_with_iterators<char* const&>::value );

	struct check_5_chars final {
		void operator()( char ) {
			++m_cch;
		}
		~check_5_chars() {
			_ASSERTEQUAL(m_cch,5u);
		}
	private:
		std::size_t m_cch = 0;
	};

	{
		char ach[] = "Hello";
		char * str=ach;
		{
			check_5_chars chk;
			tc::for_each(str, std::ref(chk));
		}
		{
			check_5_chars chk;
			tc::for_each(tc::as_const(str), std::ref(chk));
		}
		{
			check_5_chars chk;
			tc::for_each(tc_move(str), std::ref(chk));
		}
	}
	{
		char str[]="Hello";
		{
			check_5_chars chk;
			tc::for_each(str, std::ref(chk));
		}
		{
			check_5_chars chk;
			tc::for_each(tc::as_const(str), std::ref(chk));
		}
	}
	{
		char const* str="Hello";
		{
			check_5_chars chk;
			tc::for_each(str, std::ref(chk));
		}
		{
			check_5_chars chk;
			tc::for_each(tc::as_const(str), std::ref(chk));
		}
		{
			check_5_chars chk;
			tc::for_each(tc_move(str), std::ref(chk));
		}
	}
	{
		char const str[]="Hello";
		{
			check_5_chars chk;
			tc::for_each(str, std::ref(chk));
		}
		{
			check_5_chars chk;
			tc::for_each(tc::as_const(str), std::ref(chk));
		}
	}
}

UNITTESTDEF( construct_array_from_range ) {
	auto rng=tc::iota(0, 10);
	auto an=tc::explicit_cast<std::array<int, 10>>(rng);
	auto anCopy=an;
	tc::array<int&, 10> anRef(an);
	tc::for_each(rng, [&](int n) {
		_ASSERTEQUAL(tc::at(an, n), n);
		_ASSERTEQUAL(tc::at(anCopy, n), n);
		_ASSERTEQUAL(tc::at(anRef, n), n);
	});
}

struct GeneratorInt {
	using value_type = int;
	template<typename Func>
	tc::break_or_continue operator()(Func func) const& {
		RETURN_IF_BREAK( tc::continue_if_not_break(func, 1));
		RETURN_IF_BREAK( tc::continue_if_not_break(func, 6));
		RETURN_IF_BREAK( tc::continue_if_not_break(func, 3));
		RETURN_IF_BREAK( tc::continue_if_not_break(func, 4));
		return tc::continue_;
	}
};

struct GeneratorLong {
	using value_type = long;
	template<typename Func>
	tc::break_or_continue operator()(Func func) const& {
		RETURN_IF_BREAK( tc::continue_if_not_break(func, 1l));
		RETURN_IF_BREAK( tc::continue_if_not_break(func, 6l));
		RETURN_IF_BREAK( tc::continue_if_not_break(func, 3l));
		RETURN_IF_BREAK( tc::continue_if_not_break(func, 4l));
		return tc::continue_;
	}

};

struct GeneratorGeneratorInt {
	using value_type = GeneratorInt;
	template<typename Func>
	tc::break_or_continue operator()(Func func) const& {
		RETURN_IF_BREAK( tc::continue_if_not_break(func, GeneratorInt()) );
		RETURN_IF_BREAK( tc::continue_if_not_break(func, GeneratorInt()) );
		return tc::continue_;
	}
};

struct GeneratorMutableInt {
	int m_an[3] = {1,2,3};

	using reference=int&;
	using const_reference=int const&;

	template<typename Func>
	tc::break_or_continue operator()(Func func) & {
		RETURN_IF_BREAK( tc::continue_if_not_break(func, m_an[0]) );
		RETURN_IF_BREAK( tc::continue_if_not_break(func, m_an[1]) );
		RETURN_IF_BREAK( tc::continue_if_not_break(func, m_an[2]) );
		return tc::continue_;
	}
};

} // end anonymous namespace

namespace {

static_assert(
	std::is_same<
		tc::range_reference_t<GeneratorMutableInt>,
		int&
	>::value
);

static_assert(
	std::is_same<
		tc::range_reference_t<GeneratorMutableInt const>,
		int const&
	>::value
);

struct dummy_pred {
	template<typename T>
	bool operator()(T const&) noexcept;
};

static_assert(
	std::is_same<
		tc::range_value_t<decltype(
			tc::filter(
				std::declval<GeneratorMutableInt>(),
				dummy_pred()
			)
		)>,
		int
	>::value
);

static_assert(
	std::is_same<
		tc::range_value_t<std::add_const_t<decltype(
			tc::filter(
				std::declval<GeneratorMutableInt>(),
				dummy_pred()
			)
		)>>,
		int
	>::value
);

static_assert(
	std::is_same<
		tc::range_value_t<
			decltype(
				tc::filter(
					std::declval<GeneratorMutableInt&>(),
					dummy_pred()
				)
			)
		>,
		int
	>::value
);

static_assert(
	std::is_same<
		tc::range_value_t<
			std::add_const_t<decltype(
				tc::filter(
					std::declval<GeneratorMutableInt&>(),
					dummy_pred()
				)
			)>
		>,
		int
	>::value
);

static_assert(
	std::is_same<
		tc::range_value_t<
			decltype(
				tc::filter(
					std::declval<GeneratorMutableInt const&>(),
					dummy_pred()
				)
			)
		>,
		int
	>::value
);

static_assert(
	std::is_same<
		tc::range_value_t<
			std::add_const_t<decltype(
				tc::filter(
					std::declval<GeneratorMutableInt const&>(),
					dummy_pred()
				)
			)>
		>,
		int
	>::value
);

static_assert(
	std::is_same<
		tc::range_value_t<
			std::add_const_t<decltype(
				tc::concat(
					std::declval<GeneratorMutableInt&>(),
					std::declval<GeneratorMutableInt&>()
				)
			)>
		>,
		int
	>::value
);

static_assert(
	std::is_same<
		tc::range_value_t<
			decltype(
				tc::concat(
					std::declval<GeneratorInt&>(),
					std::declval<GeneratorMutableInt&>()
				)
			)
		>,
		int
	>::value
);

UNITTESTDEF(filter_with_generator_range) {
	_ASSERTEQUAL(
		tc::max_element<tc::return_value>(
			GeneratorInt()
		),
		6
	);
	
	_ASSERTEQUAL(
		tc::max_element<tc::return_value>(
			tc::filter(
				GeneratorInt(),
				[](int n) noexcept {return 1==n%2;}
			)
		),
		3
	);

	tc::for_each(
		GeneratorInt(),
		[](int&& n) noexcept {
			n += 1;
		}
	);

	_ASSERTEQUAL(
		tc::max_element<tc::return_value>(
			tc::transform(
				GeneratorInt(),
				[](int n) noexcept {return -n;}
			)
		),
		-1
	);

	auto const tr1 = tc::transform(
		GeneratorInt(),
		[](int n) noexcept {return -n;}
	);

	static_assert(
		std::is_same<
			tc::range_value_t<decltype(tr1)>,
			int
		>::value
	);

	auto const filtered = tc::filter(
		GeneratorInt(),
		[](int n) noexcept {return n>0;}
	);
	static_assert(
		std::is_same<
			tc::range_value_t<decltype(filtered)>,
			int
		>::value
	);


	auto vecn = tc::make_vector(GeneratorInt());

	{
		auto vecn2 = tc::make_vector(tc::join(GeneratorGeneratorInt()));
		_ASSERTEQUAL(
			tc::size(vecn2),
			8
		);
	}

	auto vecgenint = tc::make_vector(GeneratorGeneratorInt());
	{
		auto vecn2 = tc::make_vector(tc::join(vecgenint));
		_ASSERTEQUAL(
			tc::size(vecn2),
			8
		);
	}

	static_assert(
		std::is_same<
			tc::range_value_t<
				decltype(tc::join(vecgenint))
			>,
			int
		>::value
	);
	auto vecnx = tc::make_vector(GeneratorInt(), GeneratorInt());
	_ASSERTEQUAL(tc::size(vecnx), 8);

	static_assert(
		std::is_same<
			tc::range_value_t<
				decltype(tc::concat(GeneratorInt(), GeneratorLong()))
			>,
			long
		>::value
	);

	static_assert(
		std::is_same<
			tc::range_value_t<
				decltype(tc::concat(GeneratorLong(), GeneratorLong()))
			>,
			long
		>::value
	);
}

UNITTESTDEF(make_constexpr_array_test) {
	constexpr auto const& an = as_constexpr(tc::make_array(tc::aggregate_tag, 1, 2, 3));
	static_assert(tc::at(an, 0) == 1);
	static_assert(tc::at(an, 1) == 2);
	static_assert(tc::at(an, 2) == 3);
	static_assert(tc::size(an) == 3);
	_ASSERT(tc::equal(an, tc::iota(1, 4)));
}

}

//---- Cartesian product ------------------------------------------------------------------------------------------------------

namespace {
	[[maybe_unused]] void cartesian_product_test(std::array<int, 2>& an) {
		tc::for_each(tc::cartesian_product(an, an), [](auto&& x){
			STATICASSERTSAME(decltype(x), (tc::tuple<int&, int&>&&));
		});
		tc::for_each(tc::cartesian_product(tc::as_const(an), tc::as_const(an)), [](auto&& x){
			STATICASSERTSAME(decltype(x), (tc::tuple<int const&, const int&>&&));
		});
		auto rvaluerng = [](auto sink) { sink(1); };
		tc::for_each(tc::cartesian_product(rvaluerng, rvaluerng), [](auto&& x){
			STATICASSERTSAME(decltype(x), (tc::tuple<int const&&, int&&>&&));
		});
	}
}
