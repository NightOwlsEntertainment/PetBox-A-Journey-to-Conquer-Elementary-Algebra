/*
(c) 2014 Glen Joseph Fernandes
glenjofe at gmail dot com

Distributed under the Boost Software
License, Version 1.0.
http://boost.org/LICENSE_1_0.txt
*/
#ifndef BOOST_ALIGN_DETAIL_MAX_ALIGN_HPP
#define BOOST_ALIGN_DETAIL_MAX_ALIGN_HPP

#include <boost/align/detail/integral_constant.hpp>
#include <cstddef>

namespace boost {
namespace alignment {
namespace detail {

template<std::size_t A, std::size_t B>
struct max_align
    : integral_constant<std::size_t, (A > B) ? A : B> {
};

} /* :detail */
} /* :alignment */
} /* :boost */

#endif
