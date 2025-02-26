/*
(c) 2014 Glen Joseph Fernandes
glenjofe at gmail dot com

Distributed under the Boost Software
License, Version 1.0.
http://boost.org/LICENSE_1_0.txt
*/
#ifndef BOOST_ALIGN_DETAIL_ALIGNED_ALLOC_MACOS_HPP
#define BOOST_ALIGN_DETAIL_ALIGNED_ALLOC_MACOS_HPP

#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/align/detail/is_alignment.hpp>
#include <cstddef>
#include <stdlib.h>

namespace boost {
namespace alignment {

inline void* aligned_alloc(std::size_t alignment, std::size_t size)
    BOOST_NOEXCEPT
{
    BOOST_ASSERT(detail::is_alignment(alignment));
    if (!size) {
        return 0;
    }
    if (alignment < sizeof(void*)) {
        alignment = sizeof(void*);
    }
    void* p;
    if (::posix_memalign(&p, alignment, size) != 0) {
        p = 0;
    }
    return p;
}

inline void aligned_free(void* ptr) BOOST_NOEXCEPT
{
    ::free(ptr);
}

} /* :alignment */
} /* :boost */

#endif
