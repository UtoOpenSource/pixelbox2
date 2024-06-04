# Engine
this directory purely consists of most critical and viedly used
functions/libraries/systems all over the place :
- Random number generator + 2D noise
- multithreaded CPU profiler
- Doctest for unit testing
- Base objects implementation
- ~~Linear Allocator~~ (why i removed it? it was awesome!)
- Spinlock
- Highly used Base classes :
  - Static - object is non-moable and non-copyable. Important when you keep references/pointers on it.
	- Copyable - default one. Move and copy construction and assignment are allowed
	- Moveable - only move construction and assignment are allowed
	- Default (Deprecatd) - used in some places, == Moveable. Do not use it, should be removed soon
- locale-independent strtod()

# todo
- ~~Locale and implementation-independent vsnprintf() :p~~(done)
