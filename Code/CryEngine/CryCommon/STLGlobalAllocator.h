//---------------------------------------------------------------------------
// Copyright 2010 Crytek GmbH
// Created by: Stephen Barnett
//---------------------------------------------------------------------------
#ifndef __STLGLOBALALLOCATOR_H__
#define __STLGLOBALALLOCATOR_H__

//---------------------------------------------------------------------------
// STL-compatible interface for an std::allocator using the global heap.
//---------------------------------------------------------------------------

#include <stddef.h>
#include <climits>

#include "CryMemoryManager.h"

class ICrySizer;
namespace stl
{
	template <class T>
	class STLGlobalAllocator
	{
	public:
		typedef size_t    size_type;
		typedef ptrdiff_t difference_type;
		typedef T*        pointer;
		typedef const T*  const_pointer;
		typedef T&        reference;
		typedef const T&  const_reference;
		typedef T         value_type;

		template <class U> struct rebind
		{
			typedef STLGlobalAllocator<U> other;
		};

		STLGlobalAllocator() throw()
		{
		}

		STLGlobalAllocator(const STLGlobalAllocator&) throw()
		{
		}

		template <class U> STLGlobalAllocator(const STLGlobalAllocator<U>&) throw()
		{
		}

		~STLGlobalAllocator() throw()
		{
		}

		pointer address(reference x) const
		{
			return &x;
		}

		const_pointer address(const_reference x) const
		{
			return &x;
		}

		pointer allocate(size_type n = 1, const void* hint = 0)
		{
			(void)hint;
			ScopedSwitchToGlobalHeap useGlobalHeap;
			return static_cast<pointer>(CryModuleMalloc(n * sizeof(T)));
		}

		void deallocate(pointer p, size_type n = 1)
		{
			CryModuleFree(p);
		}

		size_type max_size() const throw()
		{
			return INT_MAX;
		}

#if defined(_MSC_VER)
		template<class U>
		void construct(U* p, const U& val)
		{
			new((void*)p) U(val);
		}

		template<class U>
		void construct(U* p)
		{
			new((void*)p) U();
		}
#else
		template<class U, class... Args>
		void construct(U* p, Args&&... args)
		{
			::new((void*)p) U(std::forward<Args>(args)...);
		}
#endif

		template<class U>
		void destroy(U* p)
		{
			p->~U();
		}

		pointer new_pointer()
		{
			return new(allocate()) T();
		}

		pointer new_pointer(const T& val)
		{
			return new(allocate()) T(val);
		}

		void delete_pointer(pointer p)
		{
			p->~T();
			deallocate(p);
		}

		bool operator==(const STLGlobalAllocator&) const {return true;}
		bool operator!=(const STLGlobalAllocator&) const {return false;}

		static void GetMemoryUsage( ICrySizer *pSizer )
		{
		}
	};

	template <> class STLGlobalAllocator<void>
	{
	public:
		typedef void* pointer;
		typedef const void* const_pointer;
		typedef void value_type;
		template <class U> 
		struct rebind { typedef STLGlobalAllocator<U> other; };
	};    
}

#endif //__STLGLOBALALLOCATOR_H__
