#include "stdafx.h"
#include <cstdlib>
#include <iostream>
#include <boost/aligned_storage.hpp>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

// Class to manage the memory to be used for handler-based custom allocation.
// It contains a single block of memory which may be returned for allocation
// requests. If the memory is in use when an allocation request is made, the
// allocator delegates allocation to the global heap.
class handler_allocator
  : private boost::noncopyable
{
public:
  handler_allocator()
    : in_use_(false)
  {
  }

  void* allocate(std::size_t size)
  {
    if (!in_use_ && size < storage_.size)
    {
      in_use_ = true;
      return storage_.address();
    }
    else
    {
      return ::operator new(size);
    }
  }

  void deallocate(void* pointer)
  {
    if (pointer == storage_.address())
    {
      in_use_ = false;
    }
    else
    {
      ::operator delete(pointer);
    }
  }

private:
  // Storage space used for handler-based custom memory allocation.
  boost::aligned_storage<2048> storage_;

  // Whether the handler-based custom allocation storage has been used.
  bool in_use_;
};

// Wrapper class template for handler objects to allow handler memory
// allocation to be customised. Calls to operator() are forwarded to the
// encapsulated handler.
template <typename Handler>
class custom_alloc_handler
{
public:
  custom_alloc_handler(handler_allocator& a, Handler h)
    : allocator_(a),
      handler_(h)
  {
  }

  void operator()()
  {
	  handler_();
  }

  template <typename Arg1>
  void operator()(Arg1 arg1)
  {
    handler_(arg1);
  }

  template <typename Arg1, typename Arg2>
  void operator()(Arg1 arg1, Arg2 arg2)
  {
    handler_(arg1, arg2);
  }

  friend void* asio_handler_allocate(std::size_t size,
      custom_alloc_handler<Handler>* this_handler)
  {
    return this_handler->allocator_.allocate(size);
  }

  friend void asio_handler_deallocate(void* pointer, std::size_t /*size*/,
      custom_alloc_handler<Handler>* this_handler)
  {
    this_handler->allocator_.deallocate(pointer);
  }

private:
  handler_allocator& allocator_;
  Handler handler_;
};

template <typename Handler>
class _custom_alloc_handler
{
public:
  _custom_alloc_handler(handler_allocator& a, Handler h)
    : allocator_(a),
      handler_(h)
  {
  }

  template <typename Arg1, typename Arg2>
  void operator()(Arg1 arg1, Arg2 arg2)
  {
    handler_(arg1, arg2);
  }

  friend void* asio_handler_allocate(std::size_t size,
      _custom_alloc_handler<Handler>* this_handler)
  {
    return this_handler->allocator_.allocate(size);
  }

  friend void asio_handler_deallocate(void* pointer, std::size_t /*size*/,
      _custom_alloc_handler<Handler>* this_handler)
  {
    this_handler->allocator_.deallocate(pointer);
  }

private:
  handler_allocator& allocator_;
  Handler handler_;
};

// Helper function to wrap a handler object to add custom allocation.
template <typename Handler>
inline custom_alloc_handler<Handler> make_custom_alloc_handler(
    handler_allocator& a, Handler h)
{
  return custom_alloc_handler<Handler>(a, h);
}

template <typename Handler>
inline _custom_alloc_handler<Handler> _make_custom_alloc_handler(
    handler_allocator& a, Handler h)
{
  return _custom_alloc_handler<Handler>(a, h);
}