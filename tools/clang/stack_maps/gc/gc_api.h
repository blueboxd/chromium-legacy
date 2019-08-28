// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_STACK_MAPS_GC_GC_API_H_
#define TOOLS_CLANG_STACK_MAPS_GC_GC_API_H_

#include <assert.h>
#include <map>
#include <ostream>
#include <vector>

using ReturnAddress = uint64_t;
using FramePtr = uintptr_t*;
using RBPOffset = uint32_t;
using DWARF = uint16_t;

// A FrameRoots object contains all the information needed to precisely identify
// live roots for a given safepoint. It contains a list of registers which are
// known to contain roots, and a list of offsets from the stack pointer to known
// on-stack-roots.
//
// Each stackmap entry in .llvm_stackmaps has two parts: a base pointer (not to
// be confused with EBP), which simply points to an object header; and a derived
// pointer which specifies an offset (if any) into the object's interior. In the
// case where only a base object pointer is desired, the derived pointer will be
// 0.
//
// DWARF Register number mapping can be found here:
// Pg.63
// https://software.intel.com/sites/default/files/article/402129/mpx-linux64-abi.pdf
class FrameRoots {
 public:
  FrameRoots(std::vector<DWARF> reg_roots, std::vector<RBPOffset> stack_roots)
      : reg_roots_(reg_roots), stack_roots_(stack_roots) {}

  const std::vector<DWARF>* reg_roots() { return &reg_roots_; }
  const std::vector<RBPOffset>* stack_roots() { return &stack_roots_; }

  bool empty() { return reg_roots_.empty() && stack_roots_.empty(); }

  friend std::ostream& operator<<(std::ostream& os, const FrameRoots& fr);

 private:
  std::vector<DWARF> reg_roots_;
  std::vector<RBPOffset> stack_roots_;
};

std::ostream& operator<<(std::ostream& os, const FrameRoots& fr);

// A SafepointTable provides a runtime mapping of function return addresses to
// on-stack and in-register gc root locations. Return addresses are used as a
// function call site is the only place where safepoints can exist. This map is
// a convenient format for the collector to use while walking a call stack
// looking for the rootset.
class SafepointTable {
 public:
  SafepointTable(std::map<ReturnAddress, FrameRoots> roots,
                 uintptr_t stack_begin)
      : roots_(std::move(roots)), stack_begin_(stack_begin) {}

  const std::map<ReturnAddress, FrameRoots>* roots() { return &roots_; }
  uintptr_t stack_begin() { return stack_begin_; }

  friend std::ostream& operator<<(std::ostream& os, const SafepointTable& st);

 private:
  std::map<ReturnAddress, FrameRoots> roots_;
  uintptr_t stack_begin_;
};

std::ostream& operator<<(std::ostream& os, const SafepointTable& st);

SafepointTable GenSafepointTable();

extern SafepointTable spt;

void PrintSafepointTable();

// Walks the execution stack looking for live gc roots. This function should
// never be called directly. Instead, the void |BeginGC| function should be
// called. |BeginGC| is an assembly shim which jumps to this function after
// placing the value of RBP in RDI (First arg slot mandated by Sys V ABI).
//
// Stack walking starts from the address in `fp` (assumed to be RBP's
// address). The stack is traversed from bottom to top until the frame pointer
// hits a terminal value (usually main's RBP value).
//
// This works by assuming the calling convention for each frame adheres to the
// Sys V ABI, where the frame pointer is known to point to the address of last
// saved frame pointer (and so on), creating a linked list of frames on the
// stack (shown below).
//
//        +--------------------+
//        |  ...               |
//        +--------------------+
//        |  Saved RBP         |<--+
//        +--------------------+   |
//        |                    |   |
//        | ...                |   |
//        |                    |   |
//        +--------------------+   |
//        |  Return Address    |   |
//        +--------------------+   |
// RBP--> |  Saved RBP         |---+
//        +--------------------+
//        |                    |
//        |  Args              |
//        |                    |
//        +--------------------+
//
// This therefore requires that the optimisation -fomit-frame-pointer is
// disabled in order to guarantee that RBP will not be used as a
// general-purpose register.
extern "C" void StackWalk(FramePtr fp);

#endif  // TOOLS_CLANG_STACK_MAPS_GC_GC_API_H_
