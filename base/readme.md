# base
this directory purely consists of most critical and viedly used
functions/libraries/systems all over the place :
- Clocksource
- Random number generator + 2D noise
- ~~Profiler (memory and CPU)~~ removed
- Doctest for unit testing
- Base objects implementation
- ~~Linear Allocator~~
- Spinlock
- Constexpr Endian detection (TODO: may not work anymore on C++23)
- Resources for multithreading (local to get resource, destruction does unlock mutex)
- Scopeguard to add quick RAII types without much of boilerplate
