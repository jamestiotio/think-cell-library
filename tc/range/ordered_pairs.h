
// think-cell public library
//
// Copyright (C) 2016-2023 think-cell Software GmbH
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include "range_adaptor.h"

namespace tc {
	namespace ordered_pairs_adaptor_adl {
		template<typename Rng>
		struct [[nodiscard]] ordered_pairs_adaptor : tc::range_adaptor_base_range<Rng> {
		private:
			using base_ = typename ordered_pairs_adaptor::range_adaptor_base_range;
		public:
			constexpr ordered_pairs_adaptor() = default;
			using base_::base_;

			template<typename Self, std::enable_if_t<tc::decayed_derived_from<Self, ordered_pairs_adaptor>>* = nullptr> // use terse syntax when Xcode supports https://cplusplus.github.io/CWG/issues/2369.html
			friend auto range_output_t_impl(Self&&) -> tc::type::list<tc::tuple<
				std::iter_reference_t<tc::iterator_t<decltype(std::declval<Self&>().base_range())>>&&,
				std::iter_reference_t<tc::iterator_t<decltype(std::declval<Self&>().base_range())>>&&
			>> {} // unevaluated

			template<tc::decayed_derived_from<ordered_pairs_adaptor> Self, typename Sink> 
			friend constexpr auto for_each_impl(Self&& self, Sink sink) MAYTHROW -> tc::common_type_t<decltype(tc::continue_if_not_break(sink, std::declval<tc::type::only_t<tc::range_output_t<Self>>>())), tc::constant<tc::continue_>> {
				auto idx1st = self.base_begin_index();
				// tc::ordered_pairs(rng) is a subsequence of tc::cartesian_product(rng, rng).
				if(!tc::at_end_index(self.base_range(), idx1st)) {
					for(;;) {
						auto idx2nd = idx1st;
						tc::increment_index(self.base_range(), idx2nd);
						if(tc::at_end_index(self.base_range(), idx2nd)) break;
						decltype(auto) ref1st = tc::dereference_index(self.base_range(), idx1st);
						do {
							tc_yield(sink, tc::forward_as_tuple(ref1st, tc::dereference_index(self.base_range(), idx2nd)));
							tc::increment_index(self.base_range(), idx2nd);
						} while(!tc::at_end_index(self.base_range(), idx2nd));
						tc::increment_index(self.base_range(), idx1st);
					}
				}
				return tc::constant<tc::continue_>();
			}

			constexpr auto size() const& noexcept requires tc::has_size<Rng> {
				std::size_t const nSize = tc::size_raw(this->base_range());
				return nSize * (nSize - 1) / 2;
			}
		};
	}

	namespace no_adl {
		template<tc::has_constexpr_size Rng>
		struct constexpr_size_impl<ordered_pairs_adaptor_adl::ordered_pairs_adaptor<Rng>> : tc::constant<tc::constexpr_size<Rng>::value * (tc::constexpr_size<Rng>::value - 1) / 2> {};
	}

	template<typename Rng>
	auto ordered_pairs(Rng&& rng) return_ctor_noexcept(
		ordered_pairs_adaptor_adl::ordered_pairs_adaptor<Rng>,
		(tc::aggregate_tag, std::forward<Rng>(rng))
	)
}
