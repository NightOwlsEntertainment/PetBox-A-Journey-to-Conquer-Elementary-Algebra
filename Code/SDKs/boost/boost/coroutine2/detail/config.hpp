
//          Copyright Oliver Kowalke 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_COROUTINES2_DETAIL_CONFIG_H
#define BOOST_COROUTINES2_DETAIL_CONFIG_H

#include <boost/config.hpp>
#include <boost/context/detail/config.hpp>
#include <boost/detail/workaround.hpp>

#ifdef BOOST_COROUTINES2_DECL
# undef BOOST_COROUTINES2_DECL
#endif

#if (defined(BOOST_ALL_DYN_LINK) || defined(BOOST_COROUTINES2_DYN_LINK) ) && ! defined(BOOST_COROUTINES2_STATIC_LINK)
# if defined(BOOST_COROUTINES2_SOURCE)
#  define BOOST_COROUTINES2_DECL BOOST_SYMBOL_EXPORT
#  define BOOST_COROUTINES2_BUILD_DLL
# else
#  define BOOST_COROUTINES2_DECL BOOST_SYMBOL_IMPORT
# endif
#endif

#if ! defined(BOOST_COROUTINES2_DECL)
# define BOOST_COROUTINES2_DECL
#endif

#if ! defined(BOOST_COROUTINES2_SOURCE) && ! defined(BOOST_ALL_NO_LIB) && ! defined(BOOST_COROUTINES2_NO_LIB)
# define BOOST_LIB_NAME boost_coroutine
# if defined(BOOST_ALL_DYN_LINK) || defined(BOOST_COROUTINES2_DYN_LINK)
#  define BOOST_DYN_LINK
# endif
# include <boost/config/auto_link.hpp>
#endif

#if defined(BOOST_USE_SEGMENTED_STACKS)
# if ! ( (defined(__GNUC__) && __GNUC__ > 3 && __GNUC_MINOR__ > 6) || \
         (defined(__clang__) && __clang_major__ > 2 && __clang_minor__ > 3) )
#  error "compiler does not support segmented_stack stacks"
# endif
# define BOOST_COROUTINES2_SEGMENTS 10
#endif

#if defined(BOOST_CONTEXT_NO_EXECUTION_CONTEXT)
# error "execution_context from boost.context not supported"
#endif

#endif // BOOST_COROUTINES2_DETAIL_CONFIG_H
