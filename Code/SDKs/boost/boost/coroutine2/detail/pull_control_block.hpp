
//          Copyright Oliver Kowalke 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_COROUTINES2_DETAIL_PULL_CONTROL_BLOCK_HPP
#define BOOST_COROUTINES2_DETAIL_PULL_CONTROL_BLOCK_HPP

#include <exception>

#include <boost/config.hpp>
#include <boost/context/execution_context.hpp>

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

namespace boost {
namespace coroutines2 {
namespace detail {

template< typename T >
struct pull_coroutine< T >::control_block {
    typename push_coroutine< T >::control_block *   other;
    boost::context::execution_context               caller;
    boost::context::execution_context               callee;
    bool                                            preserve_fpu;
    int                                             state;
    std::exception_ptr                              except;

    template< typename StackAllocator, typename Fn >
    control_block( context::preallocated, StackAllocator, Fn &&, bool);

    explicit control_block( typename push_coroutine< T >::control_block *);

    ~control_block();

    control_block( control_block &) = delete;
    control_block & operator=( control_block &) = delete;

    void resume();

    bool valid() const noexcept;
};

template< typename T >
struct pull_coroutine< T & >::control_block {
    typename push_coroutine< T & >::control_block   *   other;
    boost::context::execution_context                   caller;
    boost::context::execution_context                   callee;
    bool                                                preserve_fpu;
    int                                                 state;
    std::exception_ptr                                  except;

    template< typename StackAllocator, typename Fn >
    control_block( context::preallocated, StackAllocator, Fn &&, bool);

    explicit control_block( typename push_coroutine< T & >::control_block *);

    ~control_block();

    control_block( control_block &) = delete;
    control_block & operator=( control_block &) = delete;

    void resume();

    bool valid() const noexcept;
};

struct pull_coroutine< void >::control_block {
    push_coroutine< void >::control_block  *    other;
    boost::context::execution_context           caller;
    boost::context::execution_context           callee;
    bool                                        preserve_fpu;
    int                                         state;
    std::exception_ptr                          except;

    template< typename StackAllocator, typename Fn >
    control_block( context::preallocated, StackAllocator, Fn &&, bool);

    explicit control_block( push_coroutine< void >::control_block *);

    ~control_block();

    control_block( control_block &) = delete;
    control_block & operator=( control_block &) = delete;

    void resume();

    bool valid() const noexcept;
};

}}}

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif

#endif // BOOST_COROUTINES2_DETAIL_PULL_CONTROL_BLOCK_HPP
