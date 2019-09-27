// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains userfaultfd watcher constructor which decompress
// parts of the library's code, compressed by compress_section.py script.
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

// Symbol with virtual address of the start of ELF header of the library. Set by
// linker.
extern char __ehdr_start;

// This function can be used to prevent a value or expression from being
// optimized away by the compiler.
//
// This method is clang specific, hence the #error
#ifndef __clang__
#error "DoNotOptimize is clang specific method"
#endif
void DoNotOptimize(void* value) {
  __asm__ volatile("" : : "r,m"(value) : "memory");
}

// The following 4 arrays are here to be patched into by compress_section.py
// script. They currently contain magic bytes that will be used to locate them
// in the library file. DoNotOptimize method is applied to them at the
// beginning of the constructor to ensure that the arrays are not optimized
// away.
unsigned char g_dummy_cut_range_begin[8] = {0x2e, 0x2a, 0xee, 0xf6,
                                            0x45, 0x03, 0xd2, 0x50};
unsigned char g_dummy_cut_range_end[8] = {0x52, 0x40, 0xeb, 0x9d,
                                          0xdb, 0x11, 0xed, 0x1a};
unsigned char g_dummy_compressed_range_begin[8] = {0x5e, 0x49, 0x4a, 0x4c,
                                                   0xae, 0x28, 0xc8, 0xbb};
unsigned char g_dummy_compressed_range_end[8] = {0xdd, 0x60, 0xed, 0xcf,
                                                 0xc3, 0x29, 0xa6, 0xd6};

void* MapCutRange(void* cut_start, size_t cut_length) {
  void* addr = mmap(cut_start, cut_length, PROT_READ | PROT_EXEC | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  if (addr == MAP_FAILED) {
    perror("Constructor cut range mapping failed");
    // If we fail at this point of time there is no way for us to recover since
    // without valid mapping we can't change the cut region.
    exit(1);
  }
  return addr;
}

void* ConvertDummyArrayToAddress(unsigned char* dummy_array) {
  uintptr_t* value_ptr = (uintptr_t*)dummy_array;
  uintptr_t value = *value_ptr;
  value += (uintptr_t)&__ehdr_start;
  return (void*)value;
}

void __attribute__((constructor(100))) InitLibraryDecompressor() {
  // The constructor only works on 64 bit systems and as such expecting the
  // pointer size to be 8 bytes.
  _Static_assert(sizeof(uint64_t) == sizeof(uintptr_t),
                 "Pointer size is not 8 bytes");

  DoNotOptimize(g_dummy_cut_range_begin);
  DoNotOptimize(g_dummy_cut_range_end);
  DoNotOptimize(g_dummy_compressed_range_begin);
  DoNotOptimize(g_dummy_compressed_range_end);

  void* cut_l = ConvertDummyArrayToAddress(g_dummy_cut_range_begin);
  void* cut_r = ConvertDummyArrayToAddress(g_dummy_cut_range_end);
  void* compressed_l =
      ConvertDummyArrayToAddress(g_dummy_compressed_range_begin);
  void* compressed_r = ConvertDummyArrayToAddress(g_dummy_compressed_range_end);

  uint64_t cut_range_length = (uintptr_t)cut_r - (uintptr_t)cut_l;
  void* cut_addr = MapCutRange(cut_l, cut_range_length);

  memcpy(cut_addr, compressed_l, cut_range_length);
  if (mprotect(cut_addr, cut_range_length, PROT_READ | PROT_EXEC)) {
    perror("Failed to remove PROT_WRITE from cut range.");
    exit(1);
  }
}
