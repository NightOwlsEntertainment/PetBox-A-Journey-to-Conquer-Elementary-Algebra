/*
(c) 2015 NumScale SAS
(c) 2015 LRI UMR 8623 CNRS/University Paris Sud XI

(c) 2015 Glen Joseph Fernandes
glenjofe at gmail dot com

Distributed under the Boost Software
License, Version 1.0.
http://boost.org/LICENSE_1_0.txt
*/
#ifndef BOOST_ALIGN_ASSUME_ALIGNED_HPP
#define BOOST_ALIGN_ASSUME_ALIGNED_HPP

#include <boost/config.hpp>

#if defined(BOOST_MSVC)
#include <boost/align/detail/assume_aligned_msvc.hpp>
#elif defined(BOOST_CLANG)
#include <boost/align/detail/assume_aligned_clang.hpp>
#elif BOOST_GCC_VERSION >= 40700
#include <boost/align/detail/assume_aligned_gcc.hpp>
#elif defined(__INTEL_COMPILER)
#include <boost/align/detail/assume_aligned_intel.hpp>
#else
#include <boost/align/detail/assume_aligned.hpp>
#endif

#endif
