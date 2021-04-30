
// think-cell public library
//
// Copyright (C) 2016-2021 think-cell Software GmbH
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include "assert_defs.h"
#include "range_fwd.h"
#include "range_adaptor.h"

#include "tc_move.h"
#include "trivial_functors.h"
#include "container_traits.h"
#include "meta.h"
#include "size.h"
#include "assign.h"
#include "rvalue_property.h"
#include "optional.h"
#include "storage_for.h"
#include "modified.h"
#include "empty_range.h"

#include <type_traits>

namespace tc {
	//-------------------------------------------------------------------------------------------------------------------------
	
	template<typename T>
	constexpr T* raw_ptr(T* t) noexcept { return t; } // overloaded e.g. for boost::interprocess::offset_ptr
	
	template<typename Rng, std::enable_if_t<std::is_pointer< typename boost::range_iterator<Rng>::type >::value>* = nullptr>
	constexpr auto ptr_begin(Rng&& rng) return_decltype_NOEXCEPT(
		tc::begin(rng) // not std::forward<Rng>(rng) : there is no overload for tc::begin(Rng&&), rvalues bind to tc::begin(Rng const&)
	)

	// constexpr (1) (see above) initialization fails here.
	template<typename Rng, std::enable_if_t<std::is_pointer< typename boost::range_iterator<Rng>::type >::value>* = nullptr>
	constexpr auto ptr_end(Rng&& rng) return_decltype_NOEXCEPT(
		tc::end(rng) // not std::forward<Rng>(rng) : there is no overload for tc::end(Rng&&), rvalues bind to tc::end(Rng const&)
	)

	template<typename Rng, std::enable_if_t<!std::is_pointer< typename boost::range_iterator<Rng>::type >::value>* = nullptr>
	constexpr auto ptr_begin(Rng&& rng) return_decltype_noexcept(
		raw_ptr( rng.data() )
	)

	template<typename Rng, std::enable_if_t<!std::is_pointer< typename boost::range_iterator<Rng>::type >::value >* = nullptr>
	constexpr auto ptr_end(Rng&& rng) return_decltype_noexcept(
		raw_ptr( rng.data() ) + tc::size_raw(rng)
	)

	TC_HAS_EXPR(ptr_begin, (T), tc::ptr_begin(std::declval<T>()))
	
	//-------------------------------------------------------------------------------------------------------------------------

	namespace subrange_detail {
		template<typename Rng>
		static constexpr decltype(auto) whole_range(Rng&& rng) noexcept {
			if constexpr( tc::is_instance<tc::subrange, std::remove_reference_t<Rng>>::value ) {
				return std::forward<Rng>(rng).base_range();
			} else {
				return std::forward<Rng>(rng);
			}
		}

		template<typename Rng>
		using whole_range_t = tc::remove_rvalue_reference_t<decltype(subrange_detail::whole_range(std::declval<Rng>()))>;

		namespace no_adl {
			template<typename Rng, typename Rhs, typename = void>
			struct enable_subrange_iterator_base_ctor;

			template<typename It, typename Rhs>
			struct enable_subrange_iterator_base_ctor<iterator_base<It>, Rhs, std::enable_if_t<
				!std::is_pointer<It>::value && // not ptr_range
				!std::is_same<subrange<iterator_base<It>>, tc::remove_cvref_t<Rhs>>::value && // not copy/move ctor
				// It may not be tc::is_safely_constructible because of slicing (e.g. std::vector::const_iterator/iterator on Windows, or tc::element_t)
				std::is_constructible<It, decltype(tc::begin(std::declval<Rhs&>()))>::value &&
				std::is_constructible<It, decltype(tc::end(std::declval<Rhs&>()))>::value
			>> : std::true_type {};

			template<typename Rng, typename Rhs, typename = void>
			struct enable_ptr_range_ctor;

			template<typename TTarget, typename TSource> struct is_ptr_safely_convertible;
			template<typename T> struct is_ptr_safely_convertible<T*, T*> : std::true_type {};
			template<typename T> struct is_ptr_safely_convertible<T const*, T*> : std::true_type {};
			template<typename T> struct is_ptr_safely_convertible<T volatile*, T*> : std::true_type {};
			template<typename T> struct is_ptr_safely_convertible<T const volatile*, T*> : std::true_type {};

			template<typename T, typename Rhs>
			struct enable_ptr_range_ctor<iterator_base<T*>, Rhs, std::enable_if_t<
				!std::is_same<subrange<iterator_base<T*>>, tc::remove_cvref_t<Rhs>>::value && // not copy/move ctor
				is_ptr_safely_convertible<T*, decltype(tc::ptr_begin(std::declval<Rhs&>()))>::value &&
				is_ptr_safely_convertible<T*, decltype(tc::ptr_end(std::declval<Rhs&>()))>::value
			>> : std::true_type {};
		}
	}

	namespace no_adl {
		//-------------------------------------------------------------------------------------------------------------------------
		// meta function to determine the correct type
		// collapse subrange< subrange< ... > > to single subrange
		template<typename Rng>
		struct make_subrange_result final {
			static_assert(!std::is_rvalue_reference<Rng>::value);
			using type = subrange<subrange_detail::whole_range_t<Rng>>;
		};
	}

	namespace subrange_adl {
		template< typename Rng >
		struct subrange
			: tc::index_range_adaptor<subrange<Rng>, Rng>
			// Disable compiler generated special member functions.
			// Implicit construction from another subrange of the same type may still be possible via the user-defined constructor below.
			, std::conditional_t<
				std::is_lvalue_reference<Rng>::value || tc::is_instance<tc::iterator_base, Rng>::value,
				tc::copyable,
				std::conditional_t<
					tc::is_index_valid_for_move_constructed_range<Rng>::value,
					tc::noncopyable,
					tc::nonmovable
				>
			>
		{
			static_assert( !tc::is_instance<tc::subrange, std::remove_reference_t<Rng>>::value, "Use tc::make_subrange_result_t to construct subrange type." );

		private:
			using this_type = subrange;
			using base_ = typename subrange::index_range_adaptor;

		public:
			using typename base_::index;

		private:
			index m_idxBegin;
			index m_idxEnd;

		public:
			// default ctor (for deferred initialization)
			constexpr subrange() = default;

			// ctor from whole range
			template<ENABLE_SFINAE, typename Rhs, std::enable_if_t<std::conjunction<
				std::negation<tc::is_instance<tc::iterator_base, SFINAE_TYPE(Rng)>>,
				tc::is_safely_constructible<Rng, Rhs>
			>::value>* = nullptr>
			constexpr subrange(Rhs&& rng) noexcept(
				std::is_nothrow_constructible<base_, aggregate_tag_t, Rhs>::value &&
				noexcept(this->base_begin_index()) && std::is_nothrow_constructible<decltype(this->base_begin_index())>::value &&
				noexcept(this->base_end_index()) && std::is_nothrow_constructible<decltype(this->base_end_index())>::value
			)
				: base_(aggregate_tag, std::forward<Rhs>(rng))
				, m_idxBegin(this->base_begin_index())
				, m_idxEnd(this->base_end_index())
			{
				static_assert( !tc::is_instance<tc::subrange, std::remove_reference_t<Rhs>>::value );
			}

			template<typename Rhs, std::enable_if_t<subrange_detail::no_adl::enable_subrange_iterator_base_ctor<Rng, Rhs>::value>* = nullptr>
			constexpr subrange(Rhs&& rng) noexcept(
				noexcept(tc::begin(rng)) && std::is_nothrow_constructible<index, decltype(tc::begin(rng))>::value &&
				noexcept(tc::end(rng)) && std::is_nothrow_constructible<index, decltype(tc::end(rng))>::value
			)
				: m_idxBegin(tc::begin(rng))
				, m_idxEnd(tc::end(rng))
			{}

			template<typename Rhs, std::enable_if_t<subrange_detail::no_adl::enable_ptr_range_ctor<Rng, Rhs>::value>* = nullptr>
			constexpr subrange(Rhs&& rng) noexcept
				: m_idxBegin(tc::ptr_begin(rng))
				, m_idxEnd(tc::ptr_end(rng))
			{}

		private:
			DEFINE_NESTED_TAG_TYPE(index_tag)
			DEFINE_NESTED_TAG_TYPE(slice_tag)
			DEFINE_NESTED_TAG_TYPE(index_translation_tag)

			// ctor from rng/numeric begin/numeric end
			template<typename RhsBase, typename N>
			constexpr subrange(slice_tag_t, RhsBase&& rng, N nFrom, N nTo) noexcept
				: /*construct from whole base range*/subrange(std::forward<RhsBase>(rng))
			{
				static_assert(!tc::is_instance<tc::subrange, RhsBase>::value);
				static_assert(tc::is_random_access_range<Rng>::value); // Enables index translation in O(1).
				this->take_inplace(modified(this->begin_index(), this->advance_index(_, nTo)));
				this->drop_inplace(modified(this->begin_index(), this->advance_index(_, nFrom)));
			}

			// ctor that translates indices
			template<typename RhsBase, typename IndexBegin, typename IndexEnd>
			constexpr subrange(index_translation_tag_t, RhsBase&& baserng, IndexBegin&& idxBegin, IndexEnd&& idxEnd) noexcept
				: subrange(
					slice_tag,
					std::forward<RhsBase>(baserng),
					tc::distance_to_index(baserng, tc::begin_index(baserng), std::forward<IndexBegin>(idxBegin)),
					tc::distance_to_index(baserng, tc::begin_index(baserng), std::forward<IndexBegin>(idxEnd))
				)
			{}

			// ctor from rng/begin/end (no index translation)
			template<typename Rhs, typename IndexBegin, typename IndexEnd, std::enable_if_t<
				std::is_lvalue_reference<Rng>::value || // Implies no copy/move of base range is necessary.
				(
					!std::is_lvalue_reference<decltype(subrange_detail::whole_range(std::declval<Rhs>()))>::value && // Implies copy of base range otherwise
					!std::is_const<std::remove_reference_t<decltype(subrange_detail::whole_range(std::declval<Rhs>()))>>::value && // Implies copy of base range otherwise
					tc::is_index_valid_for_move_constructed_range<Rng>::value
				)
			>* = nullptr>
			constexpr subrange(index_tag_t, Rhs&& rng, IndexBegin&& idxBegin, IndexEnd&& idxEnd) noexcept
				: base_(aggregate_tag, subrange_detail::whole_range(std::forward<Rhs>(rng)))
				, m_idxBegin(std::forward<IndexBegin>(idxBegin))
				, m_idxEnd(std::forward<IndexEnd>(idxEnd))
			{}

			// ctor from rng/begin/end (with translation)
			template<typename Rhs, typename IndexBegin, typename IndexEnd, std::enable_if_t<
				!std::is_lvalue_reference<Rng>::value &&
				(
					std::is_lvalue_reference<decltype(subrange_detail::whole_range(std::declval<Rhs>()))>::value ||
					std::is_const<std::remove_reference_t<decltype(subrange_detail::whole_range(std::declval<Rhs>()))>>::value ||
					!tc::is_index_valid_for_move_constructed_range<Rng>::value
				)
			>* = nullptr>
			constexpr subrange(index_tag_t, Rhs&& rng, IndexBegin&& idxBegin, IndexEnd&& idxEnd) noexcept
				: subrange(index_translation_tag, subrange_detail::whole_range(std::forward<Rhs>(rng)), std::forward<IndexBegin>(idxBegin), std::forward<IndexEnd>(idxEnd))
			{}

		public:
			// ctor from other subrange ()
			template<ENABLE_SFINAE, typename Rhs, std::enable_if_t<
				!tc::is_instance<tc::iterator_base, SFINAE_TYPE(Rng)>::value &&
				(
					// Note: Substitution failure, if tc::remove_cvref_t<Rhs> not a subrange
					!std::is_same<Rng, tc::type::only_t<typename tc::is_instance<tc::subrange, std::remove_reference_t<Rhs>>::arguments>>::value ||
					(
						// Do not compete with valid, trivial copy and move ctors. But translate indices for random acess ranges, if necessary.
						!std::is_lvalue_reference<Rng>::value && tc::is_random_access_range<Rng>::value
					)
				) &&
				tc::is_safely_constructible<Rng, decltype(std::declval<Rhs>().base_range())>::value &&
				// index may not be tc::is_safely_constructible because of slicing (e.g. std::vector::const_iterator/iterator on Windows, or tc::element_t)
				std::is_constructible<index, decltype(std::declval<Rhs>().begin_index())>::value
			>* = nullptr>
			constexpr subrange(Rhs&& rng) noexcept
				: subrange(index_tag,
					std::forward<Rhs>(rng),
					std::forward<Rhs>(rng).begin_index(),
					std::forward<Rhs>(rng).end_index()
				)
			{}

			template<typename Rhs, typename Begin, typename End, std::enable_if_t<
				tc::is_safely_constructible<Rng, decltype(subrange_detail::whole_range(std::declval<Rhs>()))>::value &&
				 // index may not be tc::is_safely_constructible because of slicing (e.g. std::vector::const_iterator/iterator on Windows, or tc::element_t)
				std::is_constructible<index, decltype(tc::iterator2index(std::declval<Begin>()))>::value &&
				std::is_constructible<index, decltype(tc::iterator2index(std::declval<End>()))>::value
			>* = nullptr>
			constexpr explicit subrange(Rhs&& rng, Begin&& begin, End&& end) noexcept
				: subrange(index_tag, std::forward<Rhs>(rng), tc::iterator2index(std::forward<Begin>(begin)), tc::iterator2index(std::forward<End>(end)))
			{}

			template<ENABLE_SFINAE, std::enable_if_t<
				std::is_default_constructible<SFINAE_TYPE(base_)>::value
			>* = nullptr>
			constexpr subrange(tc::empty_range) noexcept(std::is_nothrow_default_constructible<base_>::value)
				: m_idxBegin()
				, m_idxEnd()
			{}

		private:
			template<typename Self>
			constexpr static auto data_(Self&& self) return_decltype_NOEXCEPT(
				tc::ptr_begin(self.base_range()) + tc::distance_to_index(
					self.base_range(),
					tc::begin_index(self.base_range()), // declared MAYTHROW
					self.begin_index()
				)
			)

		public:
			RVALUE_THIS_OVERLOAD_MOVABLE_MUTABLE_REF(data)

		private:
			STATIC_FINAL_MOD(constexpr, begin_index)() const& noexcept -> index {
				return m_idxBegin;
			}

			STATIC_FINAL_MOD(constexpr, end_index)() const& noexcept -> index {
				return m_idxEnd;
			}

			STATIC_FINAL_MOD(constexpr, at_end_index)(index const& idx) const& noexcept -> bool {
				return idx == m_idxEnd;
			}
		public:
			////////////////////////////////////////////////////////
			// simulate iterator interface on top of index interface

			// boost::range_iterator<subrange>::type is the same type as the base range iterator:
			using iterator = typename boost::range_iterator<decltype(*std::declval<tc::reference_or_value<Rng>&>())>::type;
			using const_iterator = typename boost::range_iterator<decltype(*std::declval<tc::reference_or_value<Rng> const&>())>::type;

			constexpr const_iterator make_iterator( index idx ) const& noexcept {
				return tc::make_iterator( this->base_range(), tc_move(idx));
			}

			constexpr const_iterator begin() const& noexcept {
				return make_iterator(this->begin_index());
			}

			constexpr const_iterator end() const& noexcept {
				return make_iterator(this->end_index());
			}

			constexpr iterator make_iterator( index idx ) & noexcept {
				return tc::make_iterator( this->base_range(), tc_move(idx));
			}

			constexpr iterator begin() & noexcept {
				return make_iterator(this->begin_index());
			}

			constexpr iterator end() & noexcept {
				return make_iterator(this->end_index());
			}

			template< typename It >
			constexpr void take_inplace( It&& it ) & noexcept {
				m_idxEnd=tc::iterator2index( std::forward<It>(it) );
			}

			template< typename It >
			constexpr void drop_inplace( It&& it ) & noexcept {
				m_idxBegin=tc::iterator2index( std::forward<It>(it) );
			}

			template< typename It >
			constexpr friend subrange&& take( subrange&& rng, It&& it ) noexcept {
				rng.take_inplace(std::forward<It>(it));
				return tc_move(rng);
			}

			template< typename It >
			constexpr friend subrange&& drop( subrange&& rng, It&& it ) noexcept {
				rng.drop_inplace(std::forward<It>(it));
				return tc_move(rng);
			}
		};
	}

	namespace no_adl {
		template<typename Rng>
		struct is_index_valid_for_move_constructed_range<tc::subrange<Rng>, std::enable_if_t<std::is_lvalue_reference<Rng>::value>> : std::true_type {};

		template<typename Rng>
		struct is_index_valid_for_move_constructed_range<tc::subrange<Rng>, std::enable_if_t<!std::is_reference<Rng>::value>> : tc::is_index_valid_for_move_constructed_range<Rng> {};
	}

	template<typename Rng, std::enable_if_t<tc::is_range_with_iterators<Rng>::value>* = nullptr>
	[[nodiscard]] auto make_view(Rng&& rng) return_ctor_MAYTHROW(
		tc::make_subrange_result_t< Rng >,
		(std::forward<Rng>(rng))
	)

	//-------------------------------------------------------------------------------------------------------------------------
	// slice

	// slice from range + iterator pair
	// slice from range + difference
	template< typename Rng, typename Begin, typename End >
	[[nodiscard]] auto slice(Rng&& rng, Begin&& begin, End&& end) return_ctor_noexcept(
		tc::make_subrange_result_t< Rng >,
		(std::forward<Rng>(rng), std::forward<Begin>(begin), std::forward<End>(end))
	)

	//-------------------------------------------------------------------------------------------------------------------------
	// take

	namespace no_adl {
		template< typename Cont, typename It, typename Enable=void >
		struct has_mem_fn_erase_from_begin final: std::false_type {};

		template< typename Cont, typename It >
		struct has_mem_fn_erase_from_begin<Cont, It, tc::void_t<decltype(std::declval<Cont&>().erase(tc::begin(std::declval<Cont&>()),std::declval<It&&>()))>> final: std::true_type {};

		template< typename Cont, typename It, typename Enable=void >
		struct has_mem_fn_erase_to_end final: std::false_type {};

		template< typename Cont, typename It >
		struct has_mem_fn_erase_to_end<Cont, It, tc::void_t<decltype(std::declval<Cont&>().erase(std::declval<It&&>(),tc::end(std::declval<Cont&>())))>> final: std::true_type {};

		template< typename Cont, typename It, typename Enable=void >
		struct has_mem_fn_take_inplace final: std::false_type {};

		template< typename Cont, typename It >
		struct has_mem_fn_take_inplace<Cont, It, tc::void_t<decltype(std::declval<Cont&>().take_inplace(std::declval<It&&>()))>> final: std::true_type {};

		template< typename Cont, typename It, typename Enable=void >
		struct has_mem_fn_drop_inplace final: std::false_type {};

		template< typename Cont, typename It >
		struct has_mem_fn_drop_inplace<Cont, It, tc::void_t<decltype(std::declval<Cont&>().drop_inplace(std::declval<It&&>()))>> final: std::true_type {};
	}

	template< typename Cont, typename It, std::enable_if_t<no_adl::has_mem_fn_erase_to_end<Cont,It>::value>* = nullptr >
	constexpr void take_inplace( Cont& cont, It&& it ) noexcept {
		cont.erase(std::forward<It>(it),tc::end(cont));
	}

	template< typename Cont, typename It, std::enable_if_t<no_adl::has_mem_fn_take_inplace<Cont,It>::value>* = nullptr >
	constexpr void take_inplace( Cont& cont, It&& it ) noexcept {
		cont.take_inplace(std::forward<It>(it));
	}

	template< typename C, typename T, typename A, typename It >
	[[nodiscard]] std::basic_string<C,T,A> && take( std::basic_string<C,T,A>&& rng, It&& it ) noexcept {
		tc::take_inplace(rng,std::forward<It>(it));
		return tc_move(rng);
	}

	template< typename Rng, typename End >
	[[nodiscard]] auto take(Rng&& rng, End&& end) return_ctor_NOEXCEPT( // boost::iterator_range doesn't have a noexcept constructor
		tc::make_subrange_result_t< Rng >,
		(std::forward<Rng>(rng), tc::begin(rng), std::forward<End>(end))
	)

	//-------------------------------------------------------------------------------------------------------------------------
	// drop
	template< typename T >
	using is_char_ptr=std::integral_constant<bool,
		std::is_pointer<
			std::decay_t<T>
		>::value &&
		tc::is_char<
			std::remove_pointer_t<std::decay_t<T>>
		>::value
	>;

	template< typename Cont, typename It, std::enable_if_t<no_adl::has_mem_fn_erase_from_begin<Cont,It>::value>* = nullptr >
	constexpr void drop_inplace( Cont & cont, It&& it ) noexcept {
		cont.erase(tc::begin(cont),std::forward<It>(it));
	}

	template< typename Cont, typename It, std::enable_if_t<no_adl::has_mem_fn_drop_inplace<Cont,It>::value>* = nullptr >
	constexpr void drop_inplace( Cont & cont, It&& it ) noexcept {
		cont.drop_inplace(std::forward<It>(it));
	}

	template< typename CharPtr, typename It, std::enable_if_t<tc::is_char_ptr<CharPtr>::value>* = nullptr>
	constexpr void drop_inplace( CharPtr& pch, It&& it ) noexcept {
		pch=std::forward<It>(it);
	}

	template< typename Rng, typename It, std::enable_if_t<!tc::is_char_ptr< Rng >::value>* = nullptr>
	constexpr tc::make_subrange_result_t< Rng > drop_impl( Rng&& rng, It&& itBegin ) noexcept {
		return tc::make_subrange_result_t< Rng >( std::forward<Rng>(rng), std::forward<It>(itBegin), tc::end(rng) );
	}

	// C strings have efficient in-place drop
	template< typename CharPtr, typename It, std::enable_if_t<tc::is_char_ptr< CharPtr >::value>* = nullptr>
	constexpr std::decay_t<CharPtr> drop_impl( CharPtr&& pch, It&& it ) noexcept {
		std::decay_t<CharPtr> pchCopy=std::forward<CharPtr>(pch);
		tc::drop_inplace( pchCopy, std::forward<It>(it) );
		return pchCopy;
	}

	template< typename Rng, typename It >
	[[nodiscard]] constexpr auto drop(Rng&& rng, It&& it) return_decltype_NOEXCEPT(
		drop_impl( std::forward<Rng>(rng), std::forward<It>(it) )
	)

	////////////////////////////////
	// front/back on ranges

	namespace begin_next_detail {
		template< typename RangeReturn, bool bLinear, typename Rng >
		constexpr auto begin_next(
			Rng&& rng,
			typename boost::range_size< std::remove_reference_t<Rng> >::type n,
			boost::iterators::forward_traversal_tag
		) noexcept {
			_ASSERTDEBUG(0 <= n);
			if constexpr(!bLinear) {
				_ASSERTENOTIFY(n <= 2);
			}
			auto it=tc::begin(rng);
			auto const itBound=tc::end(rng);
			while (0<n) {
				if(it==itBound) {
					return RangeReturn::pack_no_border(std::forward<Rng>(rng));
				}
				--n;
				++it;
			}
			return RangeReturn::pack_border(it, std::forward<Rng>(rng));
		}

		template< typename RangeReturn, bool /*bLinear*/, typename Rng >
		constexpr auto begin_next(
			Rng&& rng,
			typename boost::range_size< std::remove_reference_t<Rng> >::type n,
			boost::iterators::random_access_traversal_tag
		) noexcept {
			_ASSERTDEBUG(0<=n);
			if(n<=tc::size_raw(rng)) {
				return RangeReturn::pack_border(tc::begin(rng)+n, std::forward<Rng>(rng));
			} else {
				return RangeReturn::pack_no_border(std::forward<Rng>(rng));
			}
		}
	}

	template< typename RangeReturn, typename Rng, std::enable_if_t<tc::is_range_with_iterators<Rng>::value>* = nullptr >
	[[nodiscard]] constexpr auto begin_next(
		Rng&& rng,
		typename boost::range_size< std::remove_reference_t<Rng> >::type n=1
	) noexcept {
		return begin_next_detail::begin_next<RangeReturn, /*bLinear*/false>(std::forward<Rng>(rng), n, typename boost::range_traversal<Rng>::type());
	}

	template< typename RangeReturn, typename Rng, std::enable_if_t<tc::is_range_with_iterators<Rng>::value>* = nullptr >
	[[nodiscard]] auto linear_begin_next(
		Rng&& rng,
		typename boost::range_size< std::remove_reference_t<Rng> >::type n=1
	) noexcept {
		return begin_next_detail::begin_next<RangeReturn, /*bLinear*/true>(std::forward<Rng>(rng), n, typename boost::range_traversal<Rng>::type());
	}


	namespace end_prev_detail {
		template< typename RangeReturn, typename Rng >
		auto end_prev(
			Rng&& rng,
			typename boost::range_size< std::remove_reference_t<Rng> >::type n,
			boost::iterators::bidirectional_traversal_tag
		) noexcept {
			_ASSERT(0 <= n);
			_ASSERTNOTIFY(n <= 2);
			auto it=tc::end(rng);
			auto const itBound=tc::begin(rng);
			while (0<n) {
				if(it==itBound) {
					return RangeReturn::pack_no_border(std::forward<Rng>(rng));
				}
				--n;
				--it;
			}
			return RangeReturn::pack_border(it, std::forward<Rng>(rng));
		}

		template< typename RangeReturn, typename Rng >
		auto end_prev(
			Rng&& rng,
			typename boost::range_size< std::remove_reference_t<Rng> >::type n,
			boost::iterators::random_access_traversal_tag
		) noexcept {
			_ASSERTDEBUG(0 <= n);
			if(n<=tc::size_raw(rng)) {
				return RangeReturn::pack_border(tc::end(rng)-n, std::forward<Rng>(rng));
			} else {
				return RangeReturn::pack_no_border(std::forward<Rng>(rng));
			}
		}
	}

	template< typename RangeReturn, typename Rng >
	[[nodiscard]] auto end_prev(
		Rng&& rng,
		typename boost::range_size< std::remove_reference_t<Rng> >::type n=1
	) noexcept {
		return end_prev_detail::end_prev<RangeReturn>(std::forward<Rng>(rng), n, typename boost::range_traversal<Rng>::type());
	}

	template <typename RangeReturn, typename Rng, std::enable_if_t<RangeReturn::requires_iterator>* = nullptr>
	[[nodiscard]] constexpr decltype(auto) back(
		Rng&& rng
	) noexcept {
		if (auto it=tc::end(rng); it!=tc::begin(rng)) {
			--it;
			return RangeReturn::pack_element(it,std::forward<Rng>(rng));
		} else {
			return RangeReturn::pack_no_element(std::forward<Rng>(rng));
		}
	}

	template <typename RangeReturn, typename Rng, std::enable_if_t<RangeReturn::requires_iterator>* = nullptr>
	[[nodiscard]] constexpr decltype(auto) linear_back(
		Rng&& rng
	) noexcept {
		static_assert( !tc::is_bidirectional_range<Rng>::value, "Use tc::back for bidirectional ranges" );
		auto it = tc::begin(rng);
		auto_cref(itEnd, tc::end(rng));
		if (it!=itEnd) {
			auto itNext = it;
			while (itEnd!=++itNext) {
				it = itNext;
			}
			return RangeReturn::pack_element(it,std::forward<Rng>(rng));
		} else {
			return RangeReturn::pack_no_element(std::forward<Rng>(rng));
		}
	}

	////////////////////////////////////////////////////////////////////////////////////
	// take/drop_*_first/last_n

	namespace return_detail::no_adl {
		template<typename ReturnSubrange>
		struct return_subrange_or_assert;
	}

	namespace no_adl {
		struct return_border;
		struct return_take_or_empty;
		struct return_drop_or_empty;
	}
	using no_adl::return_border;
	using no_adl::return_take_or_empty;
	using no_adl::return_drop_or_empty;
	using return_take = return_detail::no_adl::return_subrange_or_assert<tc::return_take_or_empty>;
	using return_drop = return_detail::no_adl::return_subrange_or_assert<tc::return_drop_or_empty>;

	template< typename Cont >
	void take_first_inplace(Cont& cont, typename boost::range_size< std::remove_reference_t<Cont> >::type n=1) noexcept {
		tc::take_inplace(cont, tc::begin_next<tc::return_border>(cont,n));
	}

	template< typename It, typename T >
	T advance_forward_bounded(It&& it, T n,	tc::remove_cvref_t<It> const& itBound) noexcept;

	namespace take_first_detail {
		DEFINE_ENUM(ETakePred, etakepred,
			(TAKEANDCONTINUE)
			(TAKEANDBREAK)
			(DONTTAKE)
		)
		namespace no_adl {
			template< typename Sink, typename TakePred >
			struct take_first_sink /*final*/ : tc::sink_value_type_base<Sink> {
				static_assert(tc::is_decayed<Sink>::value);
				static_assert(tc::is_decayed<TakePred>::value);

				template<typename Sink2>
				take_first_sink(Sink2&& sink, TakePred& takepred, tc::break_or_continue& breakorcontinue) noexcept
					: m_sink(std::forward<Sink2>(sink))
					, m_takepred(takepred)
					, m_breakorcontinue(breakorcontinue)
				{}

				template< typename T >
				auto operator()(T&& t) const& MAYTHROW -> tc::common_type_t<
					decltype(tc::continue_if_not_break(std::declval<Sink const&>(), std::declval<T>())),
					INTEGRAL_CONSTANT(tc::break_)
				> {
					auto const Take=[&]() MAYTHROW { return tc::continue_if_not_break(m_sink, std::forward<T>(t)); };
					switch_no_default(m_takepred.take(t)) {
						case etakepredTAKEANDCONTINUE: {
							auto breakorcontinue=Take(); // MAYTHROW
							m_breakorcontinue=breakorcontinue;
							return breakorcontinue; // allow return type to be INTEGRAL_CONSTANT(tc::break_) if possible
						}
						case etakepredTAKEANDBREAK:
							m_breakorcontinue=Take(); // MAYTHROW
							return INTEGRAL_CONSTANT(tc::break_)();
						case etakepredDONTTAKE:
							m_breakorcontinue=tc::continue_;
							return INTEGRAL_CONSTANT(tc::break_)();
					}
				}

				template< typename Rng >
				auto chunk(Rng&& rng) const& MAYTHROW -> tc::common_type_t<
					decltype(tc::continue_if_not_break(tc::mem_fn_chunk(), std::declval<Sink const&>(), tc::take(std::declval<Rng>(), tc::begin(std::declval<Rng&>())))),
					INTEGRAL_CONSTANT(tc::break_)
				> {
					auto_cref(pairitetakepred, m_takepred.take_range(rng)); // MAYTHROW
					auto breakorcontinue=tc::continue_if_not_break(tc::mem_fn_chunk(), m_sink, tc::take(std::forward<Rng>(rng), pairitetakepred.first)); // MAYTHROW
					m_breakorcontinue=breakorcontinue;
					switch_no_default(pairitetakepred.second) {
						case etakepredTAKEANDCONTINUE: return breakorcontinue;
						case etakepredTAKEANDBREAK: return INTEGRAL_CONSTANT(tc::break_)();
					}
				}

			private:
				Sink m_sink;
				TakePred& m_takepred;
				tc::break_or_continue& m_breakorcontinue;
			};

			struct take_first_pred {
				explicit take_first_pred(std::size_t nCount) noexcept
					: m_nCount(VERIFYPRED(nCount, 0<_))
				{}

				template<typename T>
				ETakePred take(T const&) & noexcept {
					--VERIFYPRED(m_nCount, 0<_);
					return may_continue();
				}

				template<typename Rng>
				auto take_range(Rng /*const*/& rng) & MAYTHROW {
					auto it=tc::begin(rng); // MAYTHROW
					m_nCount-=tc::advance_forward_bounded(it, m_nCount, tc::end(rng)); // MAYTHROW
					return std::make_pair(it, may_continue());
				}

			protected:
				ETakePred may_continue() const& noexcept {
					return 0<m_nCount ? etakepredTAKEANDCONTINUE : etakepredTAKEANDBREAK;
				}

				std::size_t m_nCount;
			};
		}

		template< typename TakePred, bool bTruncate, typename Rng >
		auto take_first_impl(Rng&& rng, std::size_t n) noexcept {
			static_assert(!tc::is_range_with_iterators<Rng>::value);
			return [rng=tc::make_reference_or_value(std::forward<Rng>(rng)), n](auto&& sink) MAYTHROW {
				if(0<n) {
					TakePred takepred(n);
					tc::break_or_continue breakorcontinue;
					if(tc::break_==tc::for_each(*rng, no_adl::take_first_sink<tc::decay_t<decltype(sink)>, TakePred>(tc_move_if_owned(sink), takepred, breakorcontinue))) { // MAYTHROW
						return VERIFYINITIALIZED(breakorcontinue);
					} else { // breakorcontinue won't be initialized if rng is empty
						_ASSERT(bTruncate);
					}
				}
				return tc::continue_;
			};
		}
	}

	template< typename RangeReturn, typename Rng, std::enable_if_t<!tc::is_range_with_iterators<Rng>::value && std::is_same<RangeReturn, tc::return_take>::value>* = nullptr >
	[[nodiscard]] auto begin_next(Rng&& rng, std::size_t n=1) return_decltype_noexcept(
		tc::take_first_detail::take_first_impl<take_first_detail::no_adl::take_first_pred, /*bTruncate*/ false>(std::forward<Rng>(rng), n)
	)

	template< typename Cont >
	void drop_first_inplace(Cont& cont, typename boost::range_size< std::remove_reference_t<Cont> >::type n) noexcept {
		tc::drop_inplace(cont, tc::begin_next<tc::return_border>(cont, n));
	}

	template< typename Cont, std::enable_if_t<!has_mem_fn_pop_front<Cont>::value>* = nullptr>
	void drop_first_inplace(Cont& cont) noexcept {
		tc::drop_inplace(cont, tc::begin_next<tc::return_border>(cont));
	}

	template< typename Cont, std::enable_if_t<has_mem_fn_pop_front<Cont>::value>* = nullptr>
	void drop_first_inplace(Cont& cont) noexcept {
		cont.pop_front();
	}

	namespace no_adl {
		template< typename Sink >
		struct drop_first_sink /*final*/ : tc::sink_value_type_base<Sink> {
			static_assert(tc::is_decayed<Sink>::value);

			template<typename Sink2>
			drop_first_sink(Sink2&& sink, std::size_t& nCount) noexcept
				: m_sink(std::forward<Sink2>(sink))
				, m_nCount(nCount)
			{}

			template< typename T >
			auto operator()(T&& t) const& MAYTHROW -> tc::common_type_t<
				INTEGRAL_CONSTANT(tc::continue_),
				decltype(tc::continue_if_not_break(std::declval<Sink const&>(), std::declval<T>()))
			> {
				if(0<m_nCount) {
					--m_nCount;
					return INTEGRAL_CONSTANT(tc::continue_)();
				} else {
					return tc::continue_if_not_break(m_sink, std::forward<T>(t)); // MAYTHROW
				}
			}

			template< typename Rng >
			auto chunk(Rng&& rng) const& MAYTHROW -> tc::common_type_t<
				INTEGRAL_CONSTANT(tc::continue_),
				decltype(tc::continue_if_not_break(tc::mem_fn_chunk(), std::declval<Sink const&>(), tc::drop(std::declval<Rng>(), tc::begin(std::declval<Rng&>()))))
			> {
				auto it=tc::begin(rng);
				m_nCount-=tc::advance_forward_bounded(it, m_nCount, tc::end(rng));
				if(0<m_nCount) {
					_ASSERT(tc::end(rng)==it);
					return INTEGRAL_CONSTANT(tc::continue_)();
				} else {
					return tc::continue_if_not_break(tc::mem_fn_chunk(), m_sink, tc::drop(std::forward<Rng>(rng), tc_move(it))); // MAYTHROW
				}
			}

		private:
			Sink m_sink;
			std::size_t& m_nCount;
		};
	}

	template< typename RangeReturn, typename Rng, std::enable_if_t<!tc::is_range_with_iterators<Rng>::value && std::is_same<RangeReturn, tc::return_drop>::value>* = nullptr >
	[[nodiscard]] auto begin_next(Rng&& rng, std::size_t n=1) noexcept {
		return [rng=tc::make_reference_or_value(std::forward<Rng>(rng)), n](auto&& sink) MAYTHROW {
			auto nCount=n;
			auto breakorcontinue=tc::for_each(*rng, no_adl::drop_first_sink<tc::decay_t<decltype(sink)>>(tc_move_if_owned(sink), nCount)); // MAYTHROW
			_ASSERTEQUAL(nCount, 0);
			return breakorcontinue;
		};
	}

	template< typename Cont >
	void take_last_inplace(Cont& cont, typename boost::range_size< std::remove_reference_t<Cont> >::type n=1) noexcept {
		tc::drop_inplace(cont, tc::end_prev<tc::return_border>(cont, n));
	}

	template< typename Cont >
	void drop_last_inplace(Cont& cont, typename boost::range_size< std::remove_reference_t<Cont> >::type n) noexcept {
		tc::take_inplace(cont, tc::end_prev<tc::return_border>(cont, n));
	}

	template< typename Cont, std::enable_if_t<!has_mem_fn_pop_back<Cont>::value>* = nullptr>
	void drop_last_inplace(Cont& cont) noexcept {
		tc::take_inplace(cont, tc::end_prev<tc::return_border>(cont));
	}

	template< typename Cont, std::enable_if_t<has_mem_fn_pop_back<Cont>::value>* = nullptr>
	void drop_last_inplace(Cont& cont) noexcept {
		cont.pop_back();
	}

	//-------------------------------------------------------------------------------------------------------------------------
	// take_first_truncate

	template< typename It, typename T >
	T advance_forward_bounded(It&& it, T n, tc::remove_cvref_t<It> const& itBound) noexcept {
		_ASSERT(0 <= n);
		if constexpr( std::is_convertible<
			typename boost::iterator_traversal<tc::remove_cvref_t<It> >::type,
			boost::iterators::random_access_traversal_tag
		>::value ) {
			if (tc::assign_better(tc::fn_less_equal(), n, tc::make_size_proxy(itBound - it))) {
				it = itBound;
			} else {
				it += n;
			}
			return n;
		} else {
			// size_proxy does not provide operator++ and the operation cannot fail here,
			// because nCount is always inside interval [0,n].
			auto nCount = tc::explicit_cast<decltype(tc::unmake_size_proxy(n))>(0);
			while (nCount != n && it != itBound) {
				++nCount;
				++it;
			}
			RETURN_CAST(nCount);
		}
	}

	template< typename Cont >
	void take_first_truncate_inplace( Cont& rng, typename boost::range_size< std::remove_reference_t<Cont> >::type n ) noexcept {
		auto it=tc::begin(rng);
		tc::advance_forward_bounded( it, n, tc::end(rng) );
		tc::take_inplace( rng, tc_move(it) );
	}

	template< typename Rng, std::enable_if_t<!tc::is_range_with_iterators<Rng>::value>* = nullptr >
	[[nodiscard]] auto take_first_truncate(Rng&& rng, std::size_t n) return_decltype_noexcept(
		take_first_detail::take_first_impl<take_first_detail::no_adl::take_first_pred, /*bTruncate*/ true>(std::forward<Rng>(rng), n)
	)

	template< typename Rng, std::enable_if_t<tc::is_range_with_iterators<Rng>::value>* = nullptr >
	[[nodiscard]] auto take_first_truncate(Rng&& rng, typename boost::range_size< std::remove_reference_t<Rng> >::type n) noexcept
	->decltype(tc::take( std::forward<Rng>(rng), tc::begin(rng) )) {
		auto it=tc::begin(rng);
		tc::advance_forward_bounded( it, n, tc::end(rng) );
		return tc::take( std::forward<Rng>(rng), tc_move(it) );
	}

	//-------------------------------------------------------------------------------------------------------------------------
	// make iterator range

	// subrange from iterator pair
	template< typename It >
	constexpr auto make_iterator_range_impl( It itBegin, It itEnd )
		return_ctor_noexcept( tc::subrange<tc::iterator_base<It>>, ( tc::iterator_base<It>(), tc_move(itBegin), tc_move(itEnd) ) )

	// There is an other make_iterator_range_impl overload for range adaptor based iterarors in range_adaptor.h

	// make sure ADL lookup of index_iterator::make_iterator_range works
	template< typename ItBegin, typename ItEnd >
	[[nodiscard]] constexpr auto make_iterator_range(ItBegin&& itBegin, ItEnd&& itEnd) noexcept {
		return make_iterator_range_impl( std::forward<ItBegin>(itBegin), std::forward<ItEnd>(itEnd) );
	}

	//-------------------------------------------------------------------------------------------------------------------------
	// make empty range

	template< typename T >
	[[nodiscard]] constexpr auto make_empty_range() noexcept {
		return make_iterator_range( std::add_pointer_t<T>(), std::add_pointer_t<T>() );
	}
		
	//-------------------------------------------------------------------------------------------------------------------------
	// make counted range

	template< typename It, typename Count >
	[[nodiscard]] constexpr auto counted( It const& it, Count&& count ) noexcept {
		return make_iterator_range( it, it+std::forward<Count>(count) );
	}

	template< typename T, std::enable_if_t<std::is_reference<T>::value>* =nullptr >
	[[nodiscard]] constexpr auto single(T&& t) noexcept {
		return tc::counted(std::addressof(t),1);
	}

	template< typename T, std::enable_if_t<tc::is_actual_integer<T>::value && std::is_signed<T>::value>* = nullptr>
	[[nodiscard]] bool npos(T t) noexcept {
		return -1 == t;
	}

	template< typename T, std::enable_if_t<tc::is_actual_integer<T>::value && !std::is_signed<T>::value>* = nullptr>
	[[nodiscard]] bool npos(T t) noexcept {
		return std::numeric_limits<T>::max() == t;
	}

	namespace return_detail {
		template<typename Rng>
		constexpr decltype(auto) empty_slice(Rng&& rng) noexcept {
			auto it = tc::begin(rng);
			auto it2 = it;
			return tc::slice(std::forward<Rng>(rng), tc_move(it), tc_move(it2));
		}

		namespace no_adl {
			// Pack border as subrange

			struct return_take_base {
				template<typename It, typename Rng>
				static constexpr auto pack_border(It&& it, Rng&& rng) return_decltype_xvalue_by_ref_noexcept(
					tc::take(std::forward<Rng>(rng), std::forward<It>(it))
				)
			};

			struct return_drop_base {
				template<typename It, typename Rng>
				static constexpr auto pack_border(It&& it, Rng&& rng) return_decltype_noexcept(
					tc::drop(std::forward<Rng>(rng), std::forward<It>(it))
				)
			};

			template<typename ReturnSubrange>
			struct return_subrange_or_assert final : ReturnSubrange {
				static constexpr bool allowed_if_always_has_border = true;

				template<typename Rng>
				static decltype(auto) pack_no_border(Rng&& rng) noexcept {
					_ASSERTFALSE;
					return ReturnSubrange::pack_no_border(std::forward<Rng>(rng));
				}
			};

			template<typename ReturnSubrange>
			struct return_subrange_or_none final {
				static constexpr bool allowed_if_always_has_border = false;

				template<typename It, typename Rng>
				static constexpr auto pack_border(It&& it, Rng&& rng) noexcept {
					return std::optional(ReturnSubrange::pack_border(std::forward<It>(it), std::forward<Rng>(rng)));
				}

				template<typename Rng>
				static constexpr std::optional<decltype(ReturnSubrange::pack_border(tc::begin(std::declval<Rng&>()), std::declval<Rng>()))> pack_no_border(Rng&&) noexcept {
					return std::nullopt;
				}
			};

			template<typename ReturnSubrange>
			struct return_subrange_or_all final : ReturnSubrange {
				static constexpr bool allowed_if_always_has_border = false;

				template<typename Rng>
				static constexpr decltype(ReturnSubrange::pack_border(tc::begin(std::declval<Rng&>()), std::declval<Rng>())) pack_no_border(Rng&& rng) noexcept {
					return std::forward<Rng>(rng);
				}
			};

			// Pack element as border before/after

			template<typename ReturnBorder, bool bSupportsNoElement>
			struct pack_as_border_base {
				static_assert( !ReturnBorder::allowed_if_always_has_border, "Specify what to return in the no_element case. Use bSupportsNoElement=false to _ASSERT it does not occur." );
				static constexpr bool requires_iterator = true;

				template<typename Rng>
				static auto pack_no_element(Rng&& rng) noexcept code_return_decltype_xvalue_by_ref(
					if constexpr( !bSupportsNoElement ) _ASSERTFALSE;,
					ReturnBorder::pack_no_border(std::forward<Rng>(rng))
				)
			};

			template<typename ReturnBorder, bool bSupportsNoElement = true>
			struct pack_as_border_before : pack_as_border_base<ReturnBorder, bSupportsNoElement> {
				template<typename It, typename Rng, typename... Ref>
				static constexpr auto pack_element(It&& it, Rng&& rng, Ref&&...) return_decltype_xvalue_by_ref_noexcept(
					ReturnBorder::pack_border(std::forward<It>(it), std::forward<Rng>(rng))
				)
				template<typename Rng, typename Begin, typename End>
				static constexpr auto pack_view(Rng&& rng, Begin&& begin, End&& end) return_decltype_xvalue_by_ref_noexcept(
					ReturnBorder::pack_border(std::forward<Begin>(begin), std::forward<Rng>(rng))
				)
			};

			template<typename ReturnBorder, bool bSupportsNoElement = true>
			struct pack_as_border_after : pack_as_border_base<ReturnBorder, bSupportsNoElement> {
				template<typename It, typename Rng, typename... Ref>
				static constexpr auto pack_element(It&& it, Rng&& rng, Ref&&...) return_decltype_xvalue_by_ref_MAYTHROW(
					++it, // MAYTHROW
					ReturnBorder::pack_border(std::forward<It>(it), std::forward<Rng>(rng))
				)
				template<typename Rng, typename Begin, typename End>
				static constexpr auto pack_view(Rng&& rng, Begin&&, End&& end) return_decltype_xvalue_by_ref_noexcept(
					ReturnBorder::pack_border(std::forward<End>(end), std::forward<Rng>(rng))
				)
			};

			// Pack border as element before/after

			template<typename ReturnElement>
			struct pack_as_element_base {
				static constexpr bool allowed_if_always_has_border = true; // element before/after may not exist, even if we have a border.
				static_assert( ReturnElement::requires_iterator );

				template<typename Rng>
				static constexpr auto pack_no_border(Rng&& rng) return_decltype_noexcept(
					ReturnElement::pack_no_element(std::forward<Rng>(rng))
				)
			};

			template<typename ReturnElement>
			struct pack_as_element_before final : pack_as_element_base<ReturnElement> {
				template<typename It, typename Rng>
				static constexpr decltype(auto) pack_border(It&& it, Rng&& rng) noexcept(noexcept(--it)) {
					if( tc::begin(rng) != it ) {
						--it;
						return ReturnElement::pack_element(std::forward<It>(it), std::forward<Rng>(rng));
					} else {
						return ReturnElement::pack_no_element(std::forward<Rng>(rng));
					}
				}
			};

			template<typename ReturnElement>
			struct pack_as_element_after final : pack_as_element_base<ReturnElement> {
				template<typename It, typename Rng>
				static constexpr decltype(auto) pack_border(It&& it, Rng&& rng) noexcept {
					if( tc::end(rng) != it ) {
						return ReturnElement::pack_element(std::forward<It>(it), std::forward<Rng>(rng));
					} else {
						return ReturnElement::pack_no_element(std::forward<Rng>(rng));
					}
				}
			};

#ifdef _MSC_VER
			template<typename RangeReturn, typename Rng>
			struct element_return_type : tc::type::identity<decltype(RangeReturn::pack_no_element(std::declval<Rng>()))> {};
#endif
		}
	}

#ifdef _MSC_VER // MSVC has problems with decltype in alias templates.
	template<typename RangeReturn, typename Rng>
	using element_return_type_t = typename return_detail::no_adl::element_return_type<RangeReturn, Rng>::type;
#else
	template<typename RangeReturn, typename Rng>
	using element_return_type_t = decltype(RangeReturn::pack_no_element(std::declval<Rng>()));
#endif


	namespace no_adl {
		/////////////////////////////////////
		// return void

		struct return_void final {
			static constexpr bool requires_iterator = false;
			static constexpr bool allowed_if_always_has_border = true;

			template<typename It, typename Rng>
			static constexpr void pack_border(It&&, Rng&&) noexcept {}
			template<typename Rng>
			static constexpr void pack_no_border(Rng&&) noexcept {}
			template<typename It, typename Rng, typename... Ref>
			static constexpr void pack_element(It&&, Rng&&, Ref&&...) noexcept {}
			template<typename Rng, typename Ref>
			static constexpr void pack_element(Ref&& ref) noexcept {}
			template<typename Rng, typename It>
			static constexpr void pack_view(Rng&&, It&&, It&&) noexcept {}
			template<typename Rng>
			static constexpr void pack_no_element(Rng&&) noexcept {}
			template<typename Rng>
			static constexpr void pack_no_element() noexcept {}
		};

		/////////////////////////////////////
		// return bool

		struct return_bool {
			static constexpr bool requires_iterator = false;
			static constexpr bool allowed_if_always_has_border = false;

			template<typename It, typename Rng, typename... Ref>
			static constexpr bool pack_element(It&&, Rng&&, Ref&&...) noexcept {
				return true;
			}
			template<typename Rng, typename Ref>
			static constexpr bool pack_element(Ref&& ref) noexcept {
				return true;
			}
			template<typename Rng, typename It>
			static constexpr bool pack_view(Rng&&, It&&, It&&) noexcept {
				return true;
			}
			template<typename Rng>
			static constexpr bool pack_no_element(Rng&&) noexcept {
				return false;
			}
			template<typename Rng>
			static constexpr bool pack_no_element() noexcept {
				return false;
			}
			template<typename It, typename Rng>
			static constexpr bool pack_border(It&&, Rng&&) noexcept {
				return true;
			}
			template<typename Rng>
			static constexpr bool pack_no_border(Rng&&) noexcept {
				return false;
			}
		};

		/////////////////////////////////////
		// controlling bound return

		// returning bound

		struct return_border {
			static constexpr bool allowed_if_always_has_border = true;

			template<typename It, typename Rng>
			static constexpr tc::decay_t<It> pack_border(It&& it, Rng&& rng) noexcept {
				return std::forward<It>(it);
			}

			template<typename Rng>
			static auto pack_no_border(Rng&& rng) noexcept {
				_ASSERTFALSE;
				return tc::begin(rng);
			}
		};

		struct return_border_or_begin final : return_border {
			static constexpr bool allowed_if_always_has_border = false;

			template<typename Rng>
			static auto pack_no_border(Rng&& rng) noexcept {
				return tc::begin(rng);
			}
		};

		struct return_border_or_end final : return_border {
			static constexpr bool allowed_if_always_has_border = false;

			template<typename Rng>
			static auto pack_no_border(Rng&& rng) noexcept {
				return tc::end(rng);
			}
		};

		struct return_border_or_null final {
			static constexpr bool allowed_if_always_has_border = false;

			template<typename It, typename Rng>
			static constexpr auto pack_border(It&& it, Rng&&) noexcept {
				return tc::border_t<tc::decay_t<It>>(std::forward<It>(it));
			}

			template<typename Rng>
			static constexpr auto pack_no_border(Rng&&) noexcept {
				return tc::border_t<tc::decay_t<decltype(tc::begin(std::declval<Rng&>()))>>{};
			}
		};

		struct return_border_index final {
			static constexpr bool allowed_if_always_has_border = true;

			template<typename It, typename Rng>
			static constexpr auto pack_border(It&& it, Rng&& rng) noexcept {
				return tc::make_size_proxy(it - tc::begin(rng));
			}

			template<typename Rng>
			static auto pack_no_border(Rng&&) noexcept {
				_ASSERTFALSE;
				return decltype(tc::make_size_proxy(tc::begin(std::declval<Rng&>()) - tc::begin(std::declval<Rng&>())))(0);
			}
		};

		// returning range

		struct return_take_or_empty : return_detail::no_adl::return_take_base {
			static constexpr bool allowed_if_always_has_border = false;

			template<typename Rng>
			static constexpr auto pack_no_border(Rng&& rng) return_decltype_xvalue_by_ref_noexcept(
				tc::take(std::forward<Rng>(rng), tc::begin(rng))
			)
		};

		struct return_drop_or_empty : return_detail::no_adl::return_drop_base {
			static constexpr bool allowed_if_always_has_border = false;

			template<typename Rng>
			static constexpr auto pack_no_border(Rng&& rng) return_decltype_NOEXCEPT(
				tc::drop(std::forward<Rng>(rng), tc::end(rng))
			)
		};

		/////////////////////////////////////
		// controlling element return

		// returning element

		struct return_element {
			static constexpr bool requires_iterator = true;

			template<typename It, typename Rng, typename... Ref>
			static constexpr tc::decay_t<It> pack_element(It&& it, Rng&& rng, Ref&&...) noexcept {
				return std::forward<It>(it);
			}

			template<typename Rng>
			static auto pack_no_element(Rng&& rng) noexcept {
				_ASSERTFALSE;
				return tc::begin(rng);
			}
		};

		struct return_element_or_null final {
			static constexpr bool requires_iterator = true;

			template<typename It, typename Rng, typename... Ref>
			static constexpr auto pack_element(It&& it, Rng&& rng, Ref&&...) noexcept {
				return tc::make_element(std::forward<It>(it));
			}

			template<typename Rng>
			static constexpr auto pack_no_element(Rng&&) noexcept {
				return tc::element_t<tc::decay_t<decltype(tc::begin(std::declval<Rng&>()))>>{}; // value initialization to initialize pointers to nullptr
			}
		};

		struct return_element_or_front final : return_element {
			template<typename Rng>
			static constexpr auto pack_no_element(Rng&& rng) noexcept {
				return tc::begin(rng);
			}
		};

		struct return_element_or_back final : return_element {
			template<typename Rng>
			static constexpr auto pack_no_element(Rng&& rng) noexcept {
				return tc::back<return_element>(std::forward<Rng>(rng));
			}
		};

		struct return_value {
			static constexpr bool requires_iterator = false;

			template<typename It, typename Rng, typename Ref>
			static constexpr tc::range_value_t<Rng> pack_element(It&&, Rng&&, Ref&& ref) noexcept {
				return std::forward<Ref>(ref);
			}
			template<typename It, typename Rng>
			static constexpr tc::range_value_t<Rng> pack_element(It&& it, Rng&&) noexcept {
				return *std::forward<It>(it);
			}
			template<typename Rng, typename Ref>
			static constexpr tc::range_value_t<Rng> pack_element(Ref&& ref) noexcept {
				return std::forward<Ref>(ref);
			}
			template<typename Rng>
			static auto pack_no_element(Rng&&) noexcept {
				_ASSERTFALSE;
				return tc::construct_default_or_terminate<tc::range_value_t<Rng>>();
			}
			template<typename Rng>
			static auto pack_no_element() noexcept {
				_ASSERTFALSE;
				return tc::construct_default_or_terminate<tc::range_value_t<Rng>>();
			}
		};

		struct return_value_or_default final : return_value {
			template<typename Rng>
			static constexpr tc::range_value_t<Rng> pack_no_element(Rng&&) noexcept {
				return {};
			}
			template<typename Rng>
			static constexpr tc::range_value_t<Rng> pack_no_element() noexcept {
				return {};
			}
		};

		struct return_value_or_none final {
			static constexpr bool requires_iterator = false;

			template<typename It, typename Rng, typename Ref>
			static constexpr std::optional<tc::range_value_t<Rng>> pack_element(It&&, Rng&&, Ref&& ref) noexcept {
				return std::forward<Ref>(ref);
			}
			template<typename It, typename Rng>
			static constexpr std::optional<tc::range_value_t<Rng>> pack_element(It&& it, Rng&&) noexcept {
				return *std::forward<It>(it);
			}
			template<typename Rng, typename Ref>
			static constexpr std::optional<tc::range_value_t<Rng>> pack_element(Ref&& ref) noexcept {
				return std::forward<Ref>(ref);
			}
			template<typename Rng>
			static constexpr std::optional<tc::range_value_t<Rng>> pack_no_element(Rng&&) noexcept {
				return std::nullopt;
			}
			template<typename Rng>
			static constexpr std::optional<tc::range_value_t<Rng>> pack_no_element() noexcept {
				return std::nullopt;
			}
		};

		struct return_element_index {
			static constexpr bool requires_iterator = true;

			template<typename It, typename Rng, typename... Ref>
			static constexpr auto pack_element(It&& it, Rng&& rng, Ref&&...) noexcept {
				return tc::make_size_proxy(it - tc::begin(rng));
			}

			template<typename Rng>
			static auto pack_no_element(Rng&&) noexcept {
				_ASSERTFALSE;
				return decltype(tc::make_size_proxy(tc::begin(std::declval<Rng&>()) - tc::begin(std::declval<Rng&>())))(0);
			}
		};

		struct return_element_index_or_none final {
			static constexpr bool requires_iterator = true;

			template<typename It, typename Rng, typename... Ref>
			static constexpr auto pack_element(It&& it, Rng&& rng, Ref&&...) noexcept {
				return std::optional(tc::make_size_proxy(it - tc::begin(rng)));
			}
			template<typename Rng>
			static constexpr std::optional<decltype(tc::make_size_proxy(tc::begin(std::declval<Rng&>()) - tc::begin(std::declval<Rng&>())))> pack_no_element(Rng&&) noexcept {
				return std::nullopt;
			}
		};

		struct return_element_index_or_npos final : return_element_index {
			// Note: prefer return_element_index_or_none
			// static_cast<int>(npos) must be -1. Anything else is error-prone. So use range_difference instead of range_size.
			// Alternatively, we could return a special type that casts npos to -1.
			static constexpr bool requires_iterator = true;

			template<typename Rng>
			static constexpr auto pack_no_element(Rng&&) noexcept {
				return decltype(tc::make_size_proxy(tc::begin(std::declval<Rng&>()) - tc::begin(std::declval<Rng&>())))(-1);
			}
		};

		struct return_element_index_or_size final : return_element_index {
			template<typename Rng>
			static constexpr auto pack_no_element(Rng&& rng) noexcept {
				return decltype(tc::make_size_proxy(tc::begin(std::declval<Rng&>()) - tc::begin(std::declval<Rng&>())))(tc::size(rng));
			}
		};

		struct return_singleton_range {
			static constexpr bool requires_iterator = true;

			template<typename It, typename Rng, typename... Ref>
			static constexpr decltype(auto) pack_element(It&& it, Rng&& rng, Ref&&...) noexcept(noexcept(++tc::as_lvalue(tc::decay_copy(it)))) {
				return tc::slice(std::forward<Rng>(rng), it, modified(it, ++_));
			}
			template<typename Rng>
			static decltype(auto) pack_no_element(Rng&& rng) noexcept {
				// safe choice is empty because result may be empty
				_ASSERTFALSE;
				return tc::return_detail::empty_slice(std::forward<Rng>(rng));
			}
		};

		struct return_singleton_range_or_empty final : return_singleton_range {
			template<typename Rng>
			static constexpr auto pack_no_element(Rng&& rng) return_decltype_noexcept(
				tc::return_detail::empty_slice(std::forward<Rng>(rng))
			)
		};

		/////////////////////////////////////
		// controlling view return

		struct return_view {
			template<typename Rng, typename Begin, typename End>
			static constexpr auto pack_view(Rng&& rng, Begin&& begin, End&& end) return_decltype_noexcept(
				tc::slice(std::forward<Rng>(rng), std::forward<Begin>(begin), std::forward<End>(end))
			)

			template<typename Rng>
			static decltype(auto) pack_no_element(Rng&& rng) noexcept {
				_ASSERTFALSE;
				return tc::slice(std::forward<Rng>(rng), tc::begin(rng), tc::end(rng));
			}
		};

		struct return_view_or_empty final : return_view {
			template<typename Rng>
			static constexpr auto pack_no_element(Rng&& rng) return_decltype_noexcept(
				tc::return_detail::empty_slice(std::forward<Rng>(rng))
			)
		};
	} // namespace no_adl
	using no_adl::return_void;
	using no_adl::return_bool;

	// return border
	using no_adl::return_border;
	using no_adl::return_border_or_begin;
	using no_adl::return_border_or_end;
	using no_adl::return_border_or_null;
	using no_adl::return_border_index;

	using no_adl::return_take_or_empty;
	using return_take = return_detail::no_adl::return_subrange_or_assert<tc::return_take_or_empty>;
	using return_take_or_none = return_detail::no_adl::return_subrange_or_none<return_detail::no_adl::return_take_base>;
	using return_take_or_all = return_detail::no_adl::return_subrange_or_all<return_detail::no_adl::return_take_base>;

	using no_adl::return_drop_or_empty;
	using return_drop = return_detail::no_adl::return_subrange_or_assert<tc::return_drop_or_empty>;
	using return_drop_or_none = return_detail::no_adl::return_subrange_or_none<return_detail::no_adl::return_drop_base>;
	using return_drop_or_all = return_detail::no_adl::return_subrange_or_all<return_detail::no_adl::return_drop_base>;

	// return element
	using no_adl::return_element;
	using no_adl::return_element_or_null;
	using no_adl::return_element_or_front;
	using no_adl::return_element_or_back;
	using no_adl::return_value;
	using no_adl::return_value_or_default;
	using no_adl::return_value_or_none;
	using no_adl::return_element_index;
	using no_adl::return_element_index_or_none;
	using no_adl::return_element_index_or_npos;
	using no_adl::return_element_index_or_size;
	using no_adl::return_singleton_range;
	using no_adl::return_singleton_range_or_empty;

	// return border element
	using return_element_before = return_detail::no_adl::pack_as_element_before<tc::return_element>;
	using return_element_before_or_null = return_detail::no_adl::pack_as_element_before<tc::return_element_or_null>;
	using return_element_before_or_front = return_detail::no_adl::pack_as_element_before<tc::return_element_or_front>;
	using return_element_after = return_detail::no_adl::pack_as_element_after<tc::return_element>;
	using return_element_after_or_null = return_detail::no_adl::pack_as_element_after<tc::return_element_or_null>;
	using return_element_after_or_back = return_detail::no_adl::pack_as_element_after<tc::return_element_or_back>;

	// return element border
	using return_border_after = return_detail::no_adl::pack_as_border_after<tc::return_border_or_end, false>;
	using return_border_after_or_begin = return_detail::no_adl::pack_as_border_after<tc::return_border_or_begin>;
	using return_border_after_or_end = return_detail::no_adl::pack_as_border_after<tc::return_border_or_end>;
	using return_border_before = return_detail::no_adl::pack_as_border_before<tc::return_border_or_begin, false>;
	using return_border_before_or_begin = return_detail::no_adl::pack_as_border_before<tc::return_border_or_begin>;
	using return_border_before_or_end = return_detail::no_adl::pack_as_border_before<tc::return_border_or_end>;

	using return_take_before = return_detail::no_adl::pack_as_border_before<tc::return_take_or_empty, false>; // safe choice is empty because result may be empty
	using return_take_before_or_empty = return_detail::no_adl::pack_as_border_before<tc::return_take_or_empty>;
	using return_take_before_or_all = return_detail::no_adl::pack_as_border_before<tc::return_take_or_all>;
	using return_take_before_or_none = return_detail::no_adl::pack_as_border_before<tc::return_take_or_none>;
	using return_take_after = return_detail::no_adl::pack_as_border_after<tc::return_take_or_all, false>; // safe choice is all because result is never empty
	using return_take_after_or_empty = return_detail::no_adl::pack_as_border_after<tc::return_take_or_empty>;
	using return_take_after_or_all = return_detail::no_adl::pack_as_border_after<tc::return_take_or_all>;
	using return_take_after_or_none = return_detail::no_adl::pack_as_border_after<tc::return_take_or_none>;

	using return_drop_before = return_detail::no_adl::pack_as_border_before<tc::return_drop_or_all, false>; // safe choice is all because result is never empty
	using return_drop_before_or_empty = return_detail::no_adl::pack_as_border_before<tc::return_drop_or_empty>;
	using return_drop_before_or_all = return_detail::no_adl::pack_as_border_before<tc::return_drop_or_all>;
	using return_drop_before_or_none = return_detail::no_adl::pack_as_border_before<tc::return_drop_or_none>;
	using return_drop_after = return_detail::no_adl::pack_as_border_after<tc::return_drop_or_empty, false>; // safe choice is empty because result may be empty
	using return_drop_after_or_empty = return_detail::no_adl::pack_as_border_after<tc::return_drop_or_empty>;
	using return_drop_after_or_all = return_detail::no_adl::pack_as_border_after<tc::return_drop_or_all>;
	using return_drop_after_or_none = return_detail::no_adl::pack_as_border_after<tc::return_drop_or_none>;

	// return view
	using no_adl::return_view;
	using no_adl::return_view_or_empty;

	template <typename RangeReturn, typename Rng, std::enable_if_t<RangeReturn::requires_iterator>* = nullptr>
	[[nodiscard]] constexpr decltype(auto) front(
		Rng&& rng
	) noexcept {
		auto const itEnd = tc::end(rng);
		auto const itBegin = tc::begin(rng);
		if (itBegin==itEnd) {
			return RangeReturn::pack_no_element(std::forward<Rng>(rng));
		} else {
			return RangeReturn::pack_element(itBegin, std::forward<Rng>(rng));
		}
	}
	
	template <typename RangeReturn, typename Rng>
	[[nodiscard]] constexpr auto at(
		Rng&& rng,
		typename boost::range_size< std::remove_reference_t<Rng> >::type n
	) noexcept {
		if (n<tc::size_raw(rng)) {
			_ASSERTE(0<=n);
			auto it=tc::begin(rng)+n;
			return RangeReturn::pack_element(it, std::forward<Rng>(rng));
		} else {
			return RangeReturn::pack_no_element(std::forward<Rng>(rng));
		}
	}

	template <typename RangeReturn, typename Rng>
	[[nodiscard]] constexpr auto linear_at(
		Rng&& rng,
		typename boost::range_size< std::remove_reference_t<Rng> >::type n
	) noexcept {
		if constexpr (std::is_convertible<typename boost::range_traversal<Rng>::type, boost::iterators::random_access_traversal_tag>::value) {
			return tc::at<RangeReturn>(std::forward<Rng>, tc_move(n));
		} else {
			_ASSERTE(0<=n);
			auto_cref(itEnd, tc::end(rng));
			for (auto it=tc::begin(rng); it!=itEnd; ++it, --n) {
				if (0==n) {
					return RangeReturn::pack_element(it, std::forward<Rng>(rng));
				}
			}
			return RangeReturn::pack_no_element(std::forward<Rng>(rng));
		}
	}

	template <typename RangeReturn, typename Rng>
	[[nodiscard]] constexpr auto reverse_at(
		Rng&& rng,
		typename boost::range_size< std::remove_reference_t<Rng> >::type n
	) noexcept {
		if (n<tc::size_raw(rng)) {
			_ASSERTE(0<=n);
			auto it=tc::end(rng)-(n+1);
			return RangeReturn::pack_element(it, std::forward<Rng>(rng));
		} else {
			return RangeReturn::pack_no_element(std::forward<Rng>(rng));
		}
	}

	//-------------------------------------------------------------------------------------------------------------------------
	// with_optional/transform_optional/only/if_non_empty_with_[only/front]

	template<typename T, typename Func, std::enable_if_t<tc::has_bool_cast<T>::value>* = nullptr>
	bool with_optional(T&& ot, Func func) MAYTHROW {
		if(tc::bool_cast(ot)) {
			RETURNS_VOID(func(*std::forward<T>(ot))); // MAYTHROW
			return true;
		} else {
			return false;
		}
	}

	template<typename T, typename Func, std::enable_if_t<!tc::has_bool_cast<T>::value>* = nullptr>
	bool with_optional(T&& t, Func func) MAYTHROW {
		RETURNS_VOID(func(std::forward<T>(t))); // MAYTHROW
		return true;
	}

	template<typename T, typename Func>
	[[nodiscard]] auto transform_optional(T&& ot, Func func) MAYTHROW -> std::optional<
		tc::decay_t<decltype(func(*std::forward<T>(ot)))>
	> {
		if(tc::bool_cast(ot)) {
			RETURN_CAST(
				std::in_place,
				func(*std::forward<T>(ot)) // MAYTHROW
			);
		} else {
			return std::nullopt;
		}
	}

	template<typename Rng, typename Func>
	bool if_nonempty_with_only(Rng&& rng, Func func) MAYTHROW {
		bool bCalled=false;
		tc::for_each(std::forward<Rng>(rng), [&](auto&&... args) MAYTHROW {
			VERIFY(tc::change(bCalled, true)); // only a single element is allowed

			RETURNS_VOID( func(std::forward<decltype(args)>(args)...) ); // MAYTHROW

			return INTEGRAL_CONSTANT(tc::continue_)();
		});
		return bCalled;
	}

	template<typename Rng, typename Func>
	bool if_nonempty_with_front(Rng&& rng, Func func) MAYTHROW {
		return tc::break_==tc::for_each(std::forward<Rng>(rng), [&](auto&&... args) MAYTHROW{
			RETURNS_VOID(func(std::forward<decltype(args)>(args)...)); // MAYTHROW
			return INTEGRAL_CONSTANT(tc::break_)();
		});
	}

	namespace only_detail {
		template<typename RangeReturn, typename Rng, typename Func, std::enable_if_t<RangeReturn::requires_iterator>* = nullptr>
		decltype(auto) only(Rng&& rng, Func func) MAYTHROW {
			auto const itEnd = tc::end(rng);
			auto const itBegin = tc::begin(rng);
			if(itEnd != itBegin && (itEnd == modified(itBegin, ++_) || tc::bool_cast(func()))) {
				return RangeReturn::pack_element(itBegin, std::forward<Rng>(rng));
			} else {
				return RangeReturn::pack_no_element(std::forward<Rng>(rng));
			}
		}

		template<typename RangeReturn, typename Rng, typename Func, std::enable_if_t<!RangeReturn::requires_iterator>* = nullptr>
		typename tc::element_return_type_t<RangeReturn, Rng> only(Rng&& rng, Func func) MAYTHROW {
			std::optional<tc::element_return_type_t<RangeReturn, Rng>> ot;
			auto const breakorcontinue = tc::for_each(std::forward<Rng>(rng), [&](auto&& t) MAYTHROW {
				if(!ot) {
					ot.emplace(RangeReturn::template pack_element<Rng>(std::forward<decltype(t)>(t))); // MAYTHROW
					return tc::continue_;
				} else {
					return tc::break_;
				}
			});
			if(ot && (tc::continue_ == breakorcontinue || tc::bool_cast(func()))) {
				return *tc_move(ot);
			} else {
				return RangeReturn::template pack_no_element<Rng>();
			}
		}
	}

	template<typename RangeReturn, typename Rng, typename Func = tc::never_called<INTEGRAL_CONSTANT(true)>>
	[[nodiscard]] decltype(auto) only(Rng&& rng, Func&& func = Func()) MAYTHROW {
		return only_detail::only<RangeReturn>(std::forward<Rng>(rng), std::forward<Func>(func));
	}




	namespace no_adl{
		template<typename Element, typename Enable=void>
		struct element_stash final {
			static_assert(tc::is_decayed<Element>::value);
			[[nodiscard]] constexpr decltype(auto) extract(Element&& elem) const& noexcept(noexcept(*tc_move(elem))) {
#ifdef _DEBUG
				if constexpr(std::is_reference<decltype(*elem)>::value) {
					_ASSERTE(std::addressof(*tc::decay_copy(elem))==std::addressof(*elem)); // specialize is_stashing_element<Element>
				}
#endif
				return *tc_move(elem);
			}
		};

#ifdef _MSC_VER
		// If Element is of type T(*)[N], MSVC erroneously deduces return type T(*)[N] for element_stash<Element>::extract,
		// and then raises a compiler error that it cannot convert from T[N] to T(*)[N] in the return statement.
		// Clang correctly deduces return type T(&)[N]. Add specialization for array types to workaround this problem.
		template<typename T, std::size_t N>
		struct element_stash<T(*)[N], void> final {
			using array_type=T[N];
			[[nodiscard]] constexpr array_type& extract(T(*a)[N]) const& noexcept {
				return *a;
			}
		};
#endif

		// Lifetime of reference is bound to the iterator -> must keep iterator alive
		template<typename Element>
		struct element_stash<Element, std::enable_if_t<
			tc::is_stashing_element<Element>::value
		>> final : tc::storage_for_dtor_at_end_of_scope<Element> {
			static_assert(tc::is_decayed<Element>::value);
			[[nodiscard]] constexpr decltype(auto) extract(Element&& elem) & noexcept(noexcept(*tc_move_always(this->operator*()))) { // note that constexpr only works for trivially constructible/assignable/destructible type
				this->ctor(tc_move(elem));
				return *tc_move_always(this->operator*());
			}
		};
	}
	using no_adl::element_stash;

#pragma push_macro("RETURN_REFERENCE_FROM_ELEMENT")
	// Make first template argument non-type to avoid fn(rng) instantiating fn<tc::return_element, Rng> and then fn<tc::return_element, tc::return_element> which leads to hard error.
#define RETURN_REFERENCE_FROM_ELEMENT(fn) \
	template <int = 0, typename Rng, \
		typename Element=decltype(tc::fn<tc::return_element>(std::declval<Rng>())) \
	> \
	[[nodiscard]] constexpr decltype(auto) fn( \
		Rng&& rng, \
		tc::element_stash<Element>&& stash={} \
	) noexcept(noexcept(stash.extract(std::declval<Element>()))) { \
		return stash.extract(tc::fn<tc::return_element>(std::forward<Rng>(rng))); \
	}

	RETURN_REFERENCE_FROM_ELEMENT(front)
	RETURN_REFERENCE_FROM_ELEMENT(back)
	RETURN_REFERENCE_FROM_ELEMENT(linear_back)
	RETURN_REFERENCE_FROM_ELEMENT(only)
#pragma pop_macro("RETURN_REFERENCE_FROM_ELEMENT")

#pragma push_macro("RETURN_REFERENCE_FROM_INDEXED_ELEMENT")
#define RETURN_REFERENCE_FROM_INDEXED_ELEMENT(fn) \
	template <int = 0, typename Rng, \
		typename Element=decltype(tc::fn<tc::return_element>(std::declval<Rng>(), std::declval<typename boost::range_size< std::remove_reference_t<Rng> >::type>())) \
	> \
	[[nodiscard]] constexpr decltype(auto) fn( \
		Rng&& rng, \
		typename boost::range_size< std::remove_reference_t<Rng> >::type i, \
		tc::element_stash<Element>&& stash={} \
	) noexcept(noexcept(stash.extract(std::declval<Element>()))) { \
		return stash.extract(tc::fn<tc::return_element>(std::forward<Rng>(rng), tc_move(i))); \
	}

	RETURN_REFERENCE_FROM_INDEXED_ELEMENT(at)
	RETURN_REFERENCE_FROM_INDEXED_ELEMENT(linear_at)
	RETURN_REFERENCE_FROM_INDEXED_ELEMENT(reverse_at)
#pragma pop_macro("RETURN_REFERENCE_FROM_INDEXED_ELEMENT")

	// Write as macros to keep temporary iterators alive.
	// By standard, the lifetime of a reference is limited to the lifetime of the iterator.
	#define tc_front_nodebug(rng) (*tc::begin(rng))
	#define tc_at_nodebug(rng, i) *(tc::begin(rng) + static_cast<decltype(tc::begin(rng) - tc::begin(rng))>(i))


	//-------------------------------------------------------------------------------------------------------------------------
	// as_pointers
	// get a consecutive block of memory from range and return an iterator_range of pointers

	template< typename Rng, std::enable_if_t<tc::is_safely_convertible<Rng&&, tc::subrange<tc::iterator_base<decltype( ptr_begin( std::declval<Rng>() ) )>>>::value>* = nullptr >
	[[nodiscard]] constexpr auto as_pointers(Rng&& rng) noexcept ->tc::subrange<
		tc::iterator_base<
			decltype( ptr_begin( std::declval<Rng>() ) )
		>
	> {
		return std::forward<Rng>(rng);
	}

	//-------------------------------------------------------------------------------------------------------------------------
	// as_array

	template< typename T, std::size_t N, std::enable_if_t<is_char<T>::value>* = nullptr >
	[[nodiscard]] auto as_array(T (&at)[N] ) return_decltype_noexcept(
		tc::counted( std::addressof(at[0]), N )
	)

	template< typename T, std::size_t N, std::enable_if_t<is_char<T>::value && std::is_const<T>::value && !std::is_volatile<T>::value>* = nullptr >
	[[nodiscard]] constexpr auto string_literal(T (&at)[N] ) return_decltype_noexcept(
		tc::counted( std::addressof(at[0]), N-1 )
	)

	template< typename T >
	using ptr_range = subrange < iterator_base<T*> >;

	namespace no_adl {
		static_assert(
			// As long as our compiler do not correctly handle alias templates as template template parameters we need is_ptr_range
			// If this assert triggers, usage below can be replaced by
			// tc::is_instance<tc::ptr_range, std::remove_reference_t<T>>::value
			!tc::is_instance<tc::ptr_range, tc::ptr_range<int>>::value
		);

		template<typename T> struct is_ptr_range : public std::false_type {};
		template<typename T> struct is_ptr_range<T const> : public is_ptr_range<T> {};
		template<typename T> struct is_ptr_range<T volatile> : public is_ptr_range<T> {};
		template<typename T> struct is_ptr_range<T const volatile> : public is_ptr_range<T> {};
		template<typename Y> struct is_ptr_range<tc::ptr_range<Y>> : public std::true_type {};

		template<typename T>
		struct is_lvalue_reference_subrange : std::false_type {};

		template<typename Rng>
		struct is_lvalue_reference_subrange<tc::subrange<Rng>> : std::is_lvalue_reference<Rng> {};

		template<typename T, typename TSource>
		struct is_class_safely_constructible<tc::ptr_range<T>, tc::type::list<TSource>> final
			: std::integral_constant<
				bool,
				is_ptr_range<std::remove_reference_t<TSource>>::value ||
				std::is_lvalue_reference<TSource>::value ||
				std::is_pointer<std::remove_reference_t<TSource>>::value ||
				std::is_same<tc::decay_t<TSource>, tc::empty_range>::value ||
				is_lvalue_reference_subrange<tc::remove_cvref_t<TSource>>::value
			>
		{
		};

		template<typename Rng, typename Enable=void>
		struct ptr_range_type_impl final {};

		template<typename Rng>
		struct ptr_range_type_impl<Rng, std::enable_if_t<
			tc::is_safely_convertible<
				Rng,
				tc::ptr_range<std::remove_pointer_t<decltype(tc::ptr_begin(std::declval<Rng>()))>>
			>::value
		>> final {
			using type = tc::ptr_range<std::remove_pointer_t<decltype(tc::ptr_begin(std::declval<Rng>()))>>;
		};
	}

	template<typename Rng>
	using ptr_range_t = typename no_adl::ptr_range_type_impl<Rng>::type;

	namespace no_adl {
		template<typename T1, typename T2, typename Enable=void>
		struct common_ptr_range_impl_base {};

		template<typename T1, typename T2>
		struct common_ptr_range_impl_base<tc::ptr_range<T1>, tc::ptr_range<T2>, std::enable_if_t<
			sizeof(T1)==sizeof(std::remove_pointer_t<tc::common_type_t<T1*, T2*>>) &&
			sizeof(T2)==sizeof(std::remove_pointer_t<tc::common_type_t<T1*, T2*>>)
		>> {
			using type = tc::ptr_range<std::remove_pointer_t<tc::common_type_t<T1*, T2*>>>;
		};

		template<typename Rng0, typename Rng1, typename Enable=void>
		struct common_ptr_range_impl final {};

		template<>
		struct common_ptr_range_impl<empty_range, empty_range, void> final
		{
			using type = empty_range;
		};

		template<typename Rng>
		struct common_ptr_range_impl<empty_range, Rng, tc::void_t<tc::ptr_range_t<Rng>>> final
		{
			using type = tc::ptr_range_t<Rng>;
		};

		template<typename Rng>
		struct common_ptr_range_impl<Rng, empty_range, tc::void_t<tc::ptr_range_t<Rng>>> final
		{
			using type = tc::ptr_range_t<Rng>;
		};

		template<typename Rng0, typename Rng1>
		struct common_ptr_range_impl<Rng0, Rng1, tc::void_t<tc::ptr_range_t<Rng0>, tc::ptr_range_t<Rng1>>> final: common_ptr_range_impl_base<tc::ptr_range_t<Rng0>, tc::ptr_range_t<Rng1>> {};

		template<typename Rng0, typename Rng1>
		using common_ptr_range_impl_t = typename common_ptr_range_impl<Rng0, Rng1>::type;

		template<typename TypeList, typename Enable=void>
		struct has_common_ptr_range final: std::false_type {};

		template<typename TypeList>
		struct has_common_ptr_range<TypeList, tc::void_t<tc::type::accumulate_with_front_t<TypeList, common_ptr_range_impl_t>>> final: std::true_type {};

		// tc::common_reference_xvalue_as_ref_t customization tc::subrange: If
		//		1. the base ranges (whole_range_t) have a tc::common_reference_xvalue_as_ref which is an lvalue reference,
		// the result type is tc::make_subrange_result_t<tc::common_reference_xvalue_as_ref<base ranges...>>
		template<typename TypeList, typename Enable=void>
		struct common_subrange_lvalue_reference {};

		template<typename TypeList>
		struct common_subrange_lvalue_reference<TypeList, std::enable_if_t<
			std::is_lvalue_reference<
				typename common_reference_xvalue_as_ref<
					tc::type::transform_t<
						TypeList,
						subrange_detail::whole_range_t
					>
				>::type
			>::value
		>> {
			using type = tc::make_subrange_result_t<
				typename common_reference_xvalue_as_ref<
					tc::type::transform_t<
						TypeList,
						subrange_detail::whole_range_t
					>
				>::type
			>;
		};
	}
	template<typename TypeList>
	using common_subrange_lvalue_reference_t = typename no_adl::common_subrange_lvalue_reference<TypeList>::type;

	namespace no_adl {
		template<typename TypeList, typename Enable=void>
		struct has_common_subrange_lvalue_reference final: std::false_type {};

		template<typename TypeList>
		struct has_common_subrange_lvalue_reference<
			TypeList,
			tc::void_t<tc::common_subrange_lvalue_reference_t<TypeList>>
		>: std::true_type {};

		template<typename TypeList>
		struct common_reference_xvalue_as_ref<TypeList, std::enable_if_t<
			!has_guaranteed_common_reference<TypeList>::value &&
			has_common_subrange_lvalue_reference<TypeList>::value
		>> final {
			using type = tc::common_subrange_lvalue_reference_t<TypeList>;
		};

		// tc::common_reference_xvalue_as_ref_t customization tc::ptr_range: If
		//		1. all input ranges are tc::is_safely_convertible to tc::ptr_range and,
		//		2. those tc::ptr_range(s) has_common_ptr_range,
		// the result type is common_ptr_range_impl_t. 
		template<typename TypeList>
		struct common_reference_xvalue_as_ref<TypeList, std::enable_if_t<
			!has_guaranteed_common_reference<TypeList>::value &&
			!has_common_subrange_lvalue_reference<TypeList>::value &&
			has_common_ptr_range<TypeList>::value 
		>> final {
			using type = tc::type::accumulate_with_front_t<TypeList, common_ptr_range_impl_t>;
		};

	}

	//--------------------------------------------------------------------------------------------------------------------------
	// as_blob
	// reinterprets a range of items as a range of bytes

	// We use unsigned char for uninterpreted memory.
	// - The type used must support aliasing, so the only candidates are char, signed char and unsigned char.
	// - char is bad because we interpret char as UTF-8 and char* as zero-terminated range.
	// - unsigned char is better than signed char because the binary representation of signs may vary between platforms.
	// - char is bad because it is either signed or unsigned, so it has the same problem.
	// - unsigned char is better than std::uint8_t because the latter must be 8 bit, but we mean the smallest addressable unit, which is char and may be larger (or smaller?) on other platforms.

	static_assert(!tc::is_range_with_iterators< void const* >::value);

	template<typename Rng, std::enable_if_t<has_ptr_begin<Rng>::value>* = nullptr>
	[[nodiscard]] auto range_as_blob(Rng&& rng) noexcept {
		static_assert( std::is_trivially_copyable< tc::range_value_t< Rng > >::value, "as_blob only works on std::is_trivially_copyable types" );
		using cv_value_type = std::remove_pointer_t<decltype(tc::ptr_begin(rng))>;
		return tc::make_iterator_range( 
			reinterpret_cast<same_cvref_t<unsigned char, cv_value_type>*>( tc::ptr_begin(rng) ),
			reinterpret_cast<same_cvref_t<unsigned char, cv_value_type>*>( tc::ptr_end(rng) )
		);
	}

	template<typename T, std::enable_if_t<std::is_trivially_copyable<std::remove_reference_t<T>>::value>* = nullptr>
	[[nodiscard]] auto as_blob(T&& t) noexcept {
		return tc::range_as_blob( tc::single(/* no std::forward<T> */ t) );
	}

	namespace assert_no_overlap_impl {
		template< typename Lhs, typename Rhs>
		void assert_no_overlap(Lhs const& lhs, Rhs const& rhs) noexcept {
			_ASSERT(
				reinterpret_cast<std::size_t>(tc::ptr_end(lhs)) <= reinterpret_cast<std::size_t>(tc::ptr_begin(rhs)) ||
				reinterpret_cast<std::size_t>(tc::ptr_end(rhs)) <= reinterpret_cast<std::size_t>(tc::ptr_begin(lhs))
			);
		}
	}

	template< typename Lhs, typename Rhs, std::enable_if_t<!has_ptr_begin<Lhs>::value || !has_ptr_begin<Rhs>::value>* = nullptr >
	void assert_no_overlap(Lhs const& lhs, Rhs const& rhs) noexcept {
		assert_no_overlap_impl::assert_no_overlap(tc::single(lhs), tc::single(rhs));
	}

	template< typename Lhs, typename Rhs, std::enable_if_t<has_ptr_begin<Lhs>::value && has_ptr_begin<Rhs>::value>* = nullptr >
	void assert_no_overlap(Lhs const& lhs, Rhs const& rhs) noexcept {
		assert_no_overlap_impl::assert_no_overlap(tc::single(lhs), tc::single(rhs));
		assert_no_overlap_impl::assert_no_overlap(lhs, rhs);
	}

#ifdef _CHECKS
	namespace no_adl {
		template< typename Rng >
		struct SSinglePassRange : tc::noncopyable {
			explicit SSinglePassRange(Rng&& rng) noexcept
			: m_rng(tc_move(rng)) {}
			template< typename Sink >
			constexpr auto operator()(Sink&& sink) && MAYTHROW {
				_ASSERTE( tc::change(m_bFirstPass, false) );
				return tc::for_each(tc_move(m_rng), std::forward<Sink>(sink));
			}

			Rng m_rng;
			bool mutable m_bFirstPass = true;
		};
	}

	template< typename Rng >
	auto assert_single_pass(Rng&& rng) noexcept {
		return no_adl::SSinglePassRange<Rng>(tc_move(rng)); // Disallow lvalue references.
	}
#else
	template< typename Rng >
	decltype(auto) assert_single_pass(Rng&& rng) noexcept {
		return std::forward<Rng>(rng);
	}
#endif
}
