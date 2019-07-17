// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_IMMEDIATE_CRASH_H_
#define BASE_IMMEDIATE_CRASH_H_

#include "build/build_config.h"

// Crashes in the fastest possible way with no attempt at logging.
// There are several constraints; see http://crbug.com/664209 for more context.
//
// - TRAP_SEQUENCE_() must be fatal. It should not be possible to ignore the
//   resulting exception or simply hit 'continue' to skip over it in a debugger.
// - Different instances of TRAP_SEQUENCE_() must not be folded together, to
//   ensure crash reports are debuggable. Unlike __builtin_trap(), asm volatile
//   blocks will not be folded together.
//   Note: TRAP_SEQUENCE_() previously required an instruction with a unique
//   nonce since unlike clang, GCC folds together identical asm volatile
//   blocks.
// - TRAP_SEQUENCE_() must produce a signal that is distinct from an invalid
//   memory access.
// - TRAP_SEQUENCE_() must be treated as a set of noreturn instructions.
//   __builtin_unreachable() is used to provide that hint here. clang also uses
//   this as a heuristic to pack the instructions in the function epilogue to
//   improve code density.
//
// Additional properties that are nice to have:
// - TRAP_SEQUENCE_() should be as compact as possible.
// - The first instruction of TRAP_SEQUENCE_() should not change, to avoid
//   shifting crash reporting clusters. As a consequence of this, explicit
//   assembly is preferred over intrinsics.
//   Note: this last bullet point may no longer be true, and may be removed in
//   the future.

// TODO(https://crbug.com/958675): TRAP_SEQUENCE_() was previously split into
// two macro helpers to make it easier to simplify it to one instruction in
// followups. TRAP_SEQUENCE2_() will be renamed and collapsed into
// TRAP_SEQUENCE_() assuming nothing goes wrong...

#if defined(COMPILER_GCC)

#if defined(OS_NACL)

// Crash report accuracy is not guaranteed on NaCl.
#define TRAP_SEQUENCE2_() __builtin_trap()

#elif defined(ARCH_CPU_X86_FAMILY)

// In theory, it should be possible to use just int3. However, there are a
// number of crashes with SIGILL as the exception code, so it seems likely that
// there's a signal handler that allows execution to continue after SIGTRAP.

#if defined(OS_MACOSX)
// Intentionally empty: __builtin_unreachable() is always part of the sequence
// (see IMMEDIATE_CRASH below) and already emits a ud2 on Mac.
#define TRAP_SEQUENCE2_() asm volatile("")
#elif defined(OS_LINUX)
// TODO(dcheng): Remove int3 on Linux as well. Removing it is preventing
// IMMEDIATE_CRASH() from being detected as abnormal program termination on
// Linux.
#define TRAP_SEQUENCE2_() asm volatile("int3;ud2")
#else
#define TRAP_SEQUENCE2_() asm volatile("ud2")
#endif  // defined(OS_MACOSX)

#elif defined(ARCH_CPU_ARMEL)

// bkpt will generate a SIGBUS when running on armv7 and a SIGTRAP when running
// as a 32 bit userspace app on arm64. There doesn't seem to be any way to
// cause a SIGTRAP from userspace without using a syscall (which would be a
// problem for sandboxing).
// TODO(dcheng): This likely will no longer generate a SIGTRAP, update this
// comment to what it does generate?
#define TRAP_SEQUENCE2_() asm volatile("udf #0")

#elif defined(ARCH_CPU_ARM64)

// This will always generate a SIGTRAP on arm64.
// TODO(dcheng): This likely will no longer generate a SIGTRAP, update this
// comment to what it does generate?
#define TRAP_SEQUENCE2_() asm volatile("hlt #0")

#else

// Crash report accuracy will not be guaranteed on other architectures, but at
// least this will crash as expected.
#define TRAP_SEQUENCE2_() __builtin_trap()

#endif  // ARCH_CPU_*

#elif defined(COMPILER_MSVC)

#if !defined(__clang__)

// MSVC x64 doesn't support inline asm, so use the MSVC intrinsic.
#define TRAP_SEQUENCE2_() __debugbreak()

#elif defined(ARCH_CPU_ARM64)

// Intentionally empty: __builtin_unreachable() is always part of the sequence
// (see IMMEDIATE_CRASH below) and already emits a ud2 on Win64
#define TRAP_SEQUENCE2_() __asm volatile("")

#else

#if defined(ARCH_CPU_64_BITS)
// Intentionally empty: __builtin_unreachable() is always part of the sequence
// (see IMMEDIATE_CRASH below) and already emits a ud2 on Win64
#define TRAP_SEQUENCE2_() asm volatile("")
#else
#define TRAP_SEQUENCE2_() asm volatile("ud2")
#endif  // defined(ARCH_CPU_64_bits)

#endif  // __clang__

#else

#error No supported trap sequence!

#endif  // COMPILER_GCC

#define TRAP_SEQUENCE_() \
  do {                   \
    TRAP_SEQUENCE2_();   \
  } while (false)

// CHECK() and the trap sequence can be invoked from a constexpr function.
// This could make compilation fail on GCC, as it forbids directly using inline
// asm inside a constexpr function. However, it allows calling a lambda
// expression including the same asm.
// The side effect is that the top of the stacktrace will not point to the
// calling function, but to this anonymous lambda. This is still useful as the
// full name of the lambda will typically include the name of the function that
// calls CHECK() and the debugger will still break at the right line of code.
#if !defined(COMPILER_GCC)

#define WRAPPED_TRAP_SEQUENCE_() TRAP_SEQUENCE_()

#else

#define WRAPPED_TRAP_SEQUENCE_() \
  do {                           \
    [] { TRAP_SEQUENCE_(); }();  \
  } while (false)

#endif  // !defined(COMPILER_GCC)

#if defined(__clang__) || defined(COMPILER_GCC)

// __builtin_unreachable() hints to the compiler that this is noreturn and can
// be packed in the function epilogue.
#define IMMEDIATE_CRASH()     \
  ({                          \
    WRAPPED_TRAP_SEQUENCE_(); \
    __builtin_unreachable();  \
  })

#else

// This is supporting non-chromium user of logging.h to build with MSVC, like
// pdfium. On MSVC there is no __builtin_unreachable().
#define IMMEDIATE_CRASH() WRAPPED_TRAP_SEQUENCE_()

#endif  // defined(__clang__) || defined(COMPILER_GCC)

#endif  // BASE_IMMEDIATE_CRASH_H_
