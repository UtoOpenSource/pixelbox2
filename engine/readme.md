# base
this directory purely consists of most critical and viedly used
functions/libraries/systems all over the place :
- Clocksource
- Random number generator + 2D noise
- ~~Profiler (memory and CPU)~~ removed
- Doctest for unit testing
- Base objects implementation
- ~~Linear Allocator~~ (why i removed it? it was awesome!)
- Spinlock
- Constexpr Endian detection (TODO: may not work anymore on C++23)
- Resources for multithreading (local to get resource, destruction does unlock mutex)
- Scopeguard to add quick RAII types without much of boilerplate
- Highly used Base classes :
  - Static - object is non-moable and non-copyable. Important when you keep references/pointers on it.
	- Copyable - default one. Move and copy construction and assignment are allowed
	- Moveable - only move construction and assignment are allowed
	- Abstract (or virtual) - self explanotary. Used when RTTI is required or proper destruction.
	- `Shared<T>` - basically it's std::enable_shared_from_this<T>
	- Default (Deprecatd) - used in some places, == Moveable. Do not use it, should be removed soon
- HString - string with a hash inside. Kinda wonky, not always properly recalculates hash, but very useful
- locale-independent strtod()

# todo
- ~~Locale and implementation-independent vsnprintf() :p~~(done)
