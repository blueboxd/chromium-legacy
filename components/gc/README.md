# Oilpan - C++ Garbage Collection

Oilpan is an open-source garbage collection library for C++ in Chromium.

Directory structure:
- `public`: Users should depend on interfaces in the `gc` namespace exposed through this directory.
- `test`: Test utilities that are needed to write unit tests using the garbage collection library.
- `core`: The implementation of the garbage collection library. Blink may temporarily depend on concepts found in here until the public interface is complete.

The library is currently under construction. The Blink-specific parts can be found in third_party/blink/renderer/platform/heap.
