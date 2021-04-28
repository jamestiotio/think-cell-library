
// think-cell public library
//
// Copyright (C) 2016-2021 think-cell Software GmbH
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once
#include "index_range.h"
#include "assert_defs.h"
#include "iterator_facade.h"

#include <boost/iterator/detail/facade_iterator_category.hpp>

namespace tc {

	TC_HAS_EXPR(bool_cast, (T), tc::bool_cast(std::declval<T const&>()))

	namespace index_iterator_impl {

		template< typename IndexRange >
		struct delayed_difference_type {
			using type = decltype(std::declval<IndexRange const&>().distance_to_index(std::declval<typename IndexRange::index const&>(), std::declval<typename IndexRange::index const&>()));
		};

		template< typename Rng >
		struct range_traits {

			using IndexRange = std::remove_reference_t<Rng>;
			static_assert(has_index<IndexRange>::value);

			using traversal =
				std::conditional_t<tc::has_mem_fn_decrement_index<IndexRange>::value,
					std::conditional_t<tc::has_mem_fn_distance_to_index<IndexRange>::value,
						boost::iterators::random_access_traversal_tag,
						boost::iterators::bidirectional_traversal_tag
					>,
					boost::iterators::forward_traversal_tag
				>;

			using difference_type = typename std::conditional_t<
				std::is_convertible< traversal, boost::iterators::random_access_traversal_tag >::value,
				delayed_difference_type<IndexRange>,
				tc::type::identity<
					/* needed to compile interfaces relying on difference_type: */
					std::ptrdiff_t
				>
			>::type;
		};
	}

	namespace no_adl {
		template<typename It, typename Enable = void >
		struct nullable_iterator {
			static_assert(tc::is_decayed<It>::value);
			template<typename Derived>
			struct base : It {
				using difference_type = typename std::iterator_traits<It>::difference_type;
				using value_type = typename std::iterator_traits<It>::value_type;
				using pointer = typename std::iterator_traits<It>::pointer;
				using reference = typename std::iterator_traits<It>::reference;
				using iterator_category = typename std::iterator_traits<It>::iterator_category;

			private:
				Derived& derived() noexcept {return tc::derived_cast<Derived>(*this);}
			public:

				base() noexcept : m_bValid(false) {}

				// no implicit conversion from It; we must ensure that It has the semantics of element rather than border before allowing the construction
				explicit base(It const& it) noexcept : It(it), m_bValid(true) {}
				explicit base(It && it) noexcept : It(tc_move(it)), m_bValid(true) {}

				Derived& operator=(It const& it) & noexcept {
					It::operator=(it);
					m_bValid = true;
					return derived();
				}
				Derived& operator=(It && it) & noexcept {
					It::operator=(tc_move(it));
					m_bValid = true;
					return derived();
				}

				explicit operator bool() const& noexcept {
					return m_bValid;
				}

				Derived& operator++() & noexcept {
					_ASSERT(m_bValid);
					++tc::base_cast<It>(*this);
					return derived();
				}

				Derived& operator--() & noexcept {
					_ASSERT(m_bValid);
					--tc::base_cast<It>(*this);
					return derived();
				}

				reference operator*() const& noexcept {
					_ASSERT(m_bValid);
					return *tc::base_cast<It>(*this);
				}

				pointer operator->() const& noexcept {
					_ASSERT(m_bValid);
					return tc::base_cast<It>(*this).operator->();
				}

			protected:
				bool m_bValid;
			};

			struct element_type final : base<element_type> {
				using base_ = base<element_type>;
				using base_::base_;
				using base_::operator=;

				template <typename It_ = It>
				auto element_base() const& noexcept {
					using ItBase = typename nullable_iterator<decltype(tc::base_cast<It>(*this).element_base())>::element_type;
					return this->m_bValid ? ItBase{ tc::base_cast<It>(*this).element_base() } : ItBase{};
				}
			};

			struct border_type final : base<border_type> {
				using base_ = base<border_type>;
				using base_::base_;
				using base_::operator=;

				template <typename It_ = It>
				auto border_base() const& noexcept {
					using ItBase = typename nullable_iterator<decltype(tc::base_cast<It>(*this).border_base())>::border_type;
					return this->m_bValid ? ItBase{ tc::base_cast<It>(*this).border_base() } : ItBase{};
				}
			};
		};

		template<typename It>
		struct nullable_iterator< It, std::enable_if_t< has_bool_cast<It>::value > > {
			static_assert(tc::is_decayed<It>::value);
			using element_type = It;
			using border_type = It;
		};
	}

	template< typename It >
	using element_t = typename no_adl::nullable_iterator<It>::element_type;
	
	template< typename It >
	using border_t = typename no_adl::nullable_iterator<It>::border_type;

	template< typename It >
	auto make_element(It&& it) noexcept {
		return element_t<tc::decay_t<It>>(std::forward<It>(it));
	}

	template<typename T, typename Func>
	auto not_singleton_or(T&& t, Func func) noexcept->decltype(func()) {
		if(t) {
			return std::forward<T>(t);
		} else {
			return func();
		}
	}

	namespace index_iterator_impl {
		template<typename IndexRange, bool bConst>
		struct index_iterator;
	}

	struct end_sentinel final {};

	namespace index_iterator_impl {

		// Regarding constness:
		// const_iterators point to const ranges; non-const iterators point to non-const ranges.
		// The index type, traversal, and difference_type must be the same regardless of whether the range is const.
		// The reference type, and pointer type may be different.
		// The value_type should be the same but there is nothing preventing it from being different.
		// The iterator_category may be different for single-pass traversal iterators (which are not tested), since we use boost::iterators::detail::facade_iterator_category.
		
		// Other than dereference_index, base_range and creating subranges, all range functions are called on const references.

		template< typename T, bool bConst >
		using conditional_const_t=std::conditional_t< bConst, T const, T >;

		template<typename Reference, typename = void>
		struct TC_EMPTY_BASES index_iterator_base {
			using reference = Reference;
			using value_type = void;
		};

		template<typename Reference>
		struct TC_EMPTY_BASES index_iterator_base<Reference, tc::void_t<tc::decay_t<Reference>>> {
			using reference = Reference;
			using value_type = tc::decay_t<Reference>;
		};

		template<typename IndexRange, bool bConst>
		struct index_iterator
			: tc::iterator_facade<index_iterator<IndexRange, bConst>>
			, index_iterator_base<
				decltype(std::declval<conditional_const_t<IndexRange, bConst>&>().dereference_index(std::declval<index_t<IndexRange> const&>()))
			>
		{
			static_assert( tc::is_decayed< IndexRange >::value );

		private:
			friend class boost::iterator_core_access;
			friend struct index_iterator<IndexRange,!bConst>;

			conditional_const_t<IndexRange, bConst>* m_pidxrng;

		public: // TODO private
			using index = index_t<IndexRange>;
			index m_idx;

		private:
			struct enabler final {};

			using this_type = index_iterator<IndexRange, bConst>;

		public:
			using difference_type = typename range_traits< IndexRange >::difference_type; // should not be different for const and non-const
			using pointer = std::remove_reference_t<typename index_iterator::reference>*;
			using iterator_category = typename boost::iterators::detail::facade_iterator_category<typename range_traits<IndexRange>::traversal, typename index_iterator::value_type, typename index_iterator::reference>::type;
			using is_index_iterator = void;

			constexpr index_iterator() noexcept
				: m_pidxrng(nullptr)
				, m_idx()
			{}

			template<bool bConstOther>
			constexpr index_iterator(
				index_iterator<IndexRange,bConstOther> const& other
			, std::enable_if_t<
					bConst || !bConstOther
				, enabler
				> = enabler()
			) noexcept
			: m_pidxrng(other.m_pidxrng)
			, m_idx(other.m_idx) {}

			constexpr index_iterator(conditional_const_t<IndexRange, bConst>* pidxrng, index idx) noexcept
			: m_pidxrng(pidxrng)
			, m_idx(tc_move(idx)) {}

			constexpr index const& get_index() const& noexcept {
				return m_idx;
			}

			template< ENABLE_SFINAE, std::enable_if_t<tc::has_mem_fn_middle_point<SFINAE_TYPE(IndexRange)>::value>* = nullptr >
			friend index_iterator middle_point( index_iterator const& itBegin, index_iterator const& itEnd ) noexcept {
				index_iterator it=itBegin;
				_ASSERTE(itBegin.m_pidxrng);
				_ASSERTE(itBegin.m_pidxrng == itEnd.m_pidxrng);
				tc::as_const(*itBegin.m_pidxrng).middle_point(it.m_idx,itEnd.m_idx);
				return it;
			}

			template< typename IndexRange_, bool bConst1, bool bConst2,
				std::enable_if_t<tc::has_mem_fn_distance_to_index<IndexRange_>::value>*
			>
			friend constexpr typename range_traits<IndexRange_>::difference_type operator -(
				index_iterator<IndexRange_, bConst1> const& itLhs,
				index_iterator<IndexRange_, bConst2> const& itRhs) noexcept;

			template< ENABLE_SFINAE, std::enable_if_t<tc::has_mem_fn_base_range<SFINAE_TYPE(IndexRange)>::value && tc::has_mem_fn_border_base_index<IndexRange>::value>* = nullptr >
			constexpr auto border_base() const& noexcept {
				return tc::make_iterator( VERIFY(m_pidxrng)->base_range(), tc::as_const(*m_pidxrng).border_base_index(m_idx));
			}

			template< ENABLE_SFINAE, std::enable_if_t<tc::has_mem_fn_base_range<SFINAE_TYPE(IndexRange)>::value && tc::has_mem_fn_element_base_index<IndexRange>::value>* = nullptr >
			constexpr auto element_base() const& noexcept {
				using It=tc::element_t< tc::decay_t< decltype( tc::make_iterator( VERIFY(m_pidxrng)->base_range(), tc::as_const(*m_pidxrng).element_base_index(m_idx)) ) > >;
				if(m_pidxrng) {
					return It( tc::make_iterator( VERIFY(m_pidxrng)->base_range(), tc::as_const(*m_pidxrng).element_base_index(m_idx)) );
				} else {
					return It();
				}
			}

			constexpr decltype(auto) operator*() const noexcept(noexcept(m_pidxrng->dereference_index(m_idx))) {
				return VERIFY(m_pidxrng)->dereference_index(m_idx);
			}

			constexpr this_type& operator++() noexcept(noexcept(m_pidxrng->increment_index(m_idx))) {
				tc::as_const(*VERIFY(m_pidxrng)).increment_index(m_idx);
				return *this;
			}

			template<ENABLE_SFINAE, std::enable_if_t<tc::has_mem_fn_decrement_index<SFINAE_TYPE(IndexRange)>::value>* = nullptr>
			constexpr this_type& operator--() noexcept(noexcept(m_pidxrng->decrement_index(m_idx))) {
				tc::as_const(*VERIFY(m_pidxrng)).decrement_index(m_idx);
				return *this;
			}

			// subrange from iterator pair
			template<typename IndexRange_, bool bConst_>
			friend tc::make_subrange_result_t< conditional_const_t<IndexRange_,bConst_> & > make_iterator_range_impl( index_iterator<IndexRange_, bConst_> itBegin, index_iterator<IndexRange_, bConst_> itEnd ) noexcept;

			explicit constexpr operator bool() const& noexcept {
				return tc::bool_cast(m_pidxrng);
			}

			friend constexpr bool operator==(index_iterator const& it, end_sentinel) noexcept {
				return tc::as_const(*VERIFY(it.m_pidxrng)).at_end_index(it.m_idx);
			}
			friend constexpr bool operator==(end_sentinel, index_iterator const& it) noexcept {
				return tc::as_const(*VERIFY(it.m_pidxrng)).at_end_index(it.m_idx);
			}
			friend constexpr bool operator!=(index_iterator const& it, end_sentinel) noexcept {
				return !tc::as_const(*VERIFY(it.m_pidxrng)).at_end_index(it.m_idx);
			}
			friend constexpr bool operator!=(end_sentinel, index_iterator const& it) noexcept {
				return !tc::as_const(*VERIFY(it.m_pidxrng)).at_end_index(it.m_idx);
			}

			template<ENABLE_SFINAE, std::enable_if_t<tc::has_mem_fn_advance_index<SFINAE_TYPE(IndexRange)>::value>* = nullptr>
			constexpr this_type& operator+=(difference_type n) {
				tc::as_const(*VERIFY(m_pidxrng)).advance_index(m_idx, n);
				return *this;
			}

			template< typename IndexRange_, bool bConst1, bool bConst2,
				std::enable_if_t<tc::is_equality_comparable<tc::index_t<IndexRange_>>::value>*
			>
			friend constexpr bool operator==(index_iterator<IndexRange_, bConst1> const& itLhs, index_iterator<IndexRange_, bConst2> const& itRhs) noexcept;
		};

		template<typename IndexRange, bool bConst>
		tc::make_subrange_result_t< conditional_const_t<IndexRange,bConst> & > make_iterator_range_impl( index_iterator<IndexRange, bConst> itBegin, index_iterator<IndexRange, bConst> itEnd ) noexcept {
			return tc::make_subrange_result_t< conditional_const_t<IndexRange,bConst> & >( *VERIFYEQUALNOPRINT(VERIFY(itBegin.m_pidxrng),itEnd.m_pidxrng), tc_move(itBegin).m_idx, tc_move(itEnd).m_idx );
		}

		template< typename IndexRange_, bool bConst1, bool bConst2,
			std::enable_if_t<tc::is_equality_comparable<tc::index_t<IndexRange_>>::value>* = nullptr
		>
		constexpr bool operator==(index_iterator<IndexRange_, bConst1> const& itLhs, index_iterator<IndexRange_, bConst2> const& itRhs) noexcept {
			_ASSERTE(itLhs.m_pidxrng == itRhs.m_pidxrng);
			_ASSERTDEBUG( itLhs.m_pidxrng || itLhs.m_idx == itRhs.m_idx );
			return itLhs.m_idx == itRhs.m_idx;
		}

		template< typename IndexRange_, bool bConst1, bool bConst2,
			std::enable_if_t<tc::has_mem_fn_distance_to_index<IndexRange_>::value>* = nullptr
		>
		constexpr typename range_traits<IndexRange_>::difference_type operator -(
			index_iterator<IndexRange_, bConst1> const& itLhs,
			index_iterator<IndexRange_, bConst2> const& itRhs) noexcept {
			_ASSERTE(itLhs.m_pidxrng);
			_ASSERTE(itLhs.m_pidxrng == itRhs.m_pidxrng);
			return tc::as_const(*itLhs.m_pidxrng).distance_to_index(itRhs.m_idx, itLhs.m_idx);
		}

	}
	using index_iterator_impl::index_iterator;

	namespace no_adl {
		template <typename IndexRange, bool b>
		struct is_stashing_element<tc::index_iterator<IndexRange,b>> : INTEGRAL_CONSTANT(IndexRange::c_bHasStashingIndex) {};
	}

	namespace iterator2index_detail {
		BOOST_MPL_HAS_XXX_TRAIT_DEF(is_index_iterator)
	}

	template<typename It, std::enable_if_t<!iterator2index_detail::has_is_index_iterator<std::remove_reference_t<It>>::value>* = nullptr>
	constexpr decltype(auto) iterator2index(It&& it) noexcept {
		return std::forward<It>(it);
	}

	template<typename It, std::enable_if_t<iterator2index_detail::has_is_index_iterator<std::remove_reference_t<It>>::value>* = nullptr>
	constexpr auto iterator2index(It&& it) return_decltype_xvalue_by_ref_noexcept(
		std::forward<It>(it).m_idx
	)
}
