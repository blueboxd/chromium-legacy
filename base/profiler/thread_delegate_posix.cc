// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <pthread.h>

#include "base/process/process_handle.h"
#include "base/profiler/thread_delegate_posix.h"

#include "build/build_config.h"

namespace base {

namespace {

uintptr_t GetThreadStackBaseAddressImpl(
    SamplingProfilerThreadToken thread_token) {
  pthread_attr_t attr;
  pthread_getattr_np(thread_token.pthread_id, &attr);
  // See crbug.com/617730 for limitations of this approach on Linux.
  void* address;
  size_t size;
  pthread_attr_getstack(&attr, &address, &size);
  const uintptr_t base_address = reinterpret_cast<uintptr_t>(address) + size;
  return base_address;
}

uintptr_t GetThreadStackBaseAddress(SamplingProfilerThreadToken thread_token) {
#if defined(OS_ANDROID)
  // Caches the main thread base address on Android since Bionic has to read
  // /proc/$PID/maps to obtain it. Other thread base addresses are sourced from
  // pthread state so are cheap to get.
  const bool is_main_thread = thread_token.id == GetCurrentProcId();
  if (is_main_thread) {
    static const uintptr_t main_thread_base_address =
        GetThreadStackBaseAddressImpl(thread_token);
    return main_thread_base_address;
  }
#endif
  return GetThreadStackBaseAddressImpl(thread_token);
}

}  // namespace

ThreadDelegatePosix::ThreadDelegatePosix(
    SamplingProfilerThreadToken thread_token)
    : thread_id_(thread_token.id),
      thread_stack_base_address_(GetThreadStackBaseAddress(thread_token)) {}

PlatformThreadId ThreadDelegatePosix::GetThreadId() const {
  return thread_id_;
}

uintptr_t ThreadDelegatePosix::GetStackBaseAddress() const {
  return thread_stack_base_address_;
}

std::vector<uintptr_t*> ThreadDelegatePosix::GetRegistersToRewrite(
    RegisterContext* thread_context) {
#if defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_32_BITS)
  return {
      reinterpret_cast<uintptr_t*>(&thread_context->arm_r0),
      reinterpret_cast<uintptr_t*>(&thread_context->arm_r1),
      reinterpret_cast<uintptr_t*>(&thread_context->arm_r2),
      reinterpret_cast<uintptr_t*>(&thread_context->arm_r3),
      reinterpret_cast<uintptr_t*>(&thread_context->arm_r4),
      reinterpret_cast<uintptr_t*>(&thread_context->arm_r5),
      reinterpret_cast<uintptr_t*>(&thread_context->arm_r6),
      reinterpret_cast<uintptr_t*>(&thread_context->arm_r7),
      reinterpret_cast<uintptr_t*>(&thread_context->arm_r8),
      reinterpret_cast<uintptr_t*>(&thread_context->arm_r9),
      reinterpret_cast<uintptr_t*>(&thread_context->arm_r10),
      reinterpret_cast<uintptr_t*>(&thread_context->arm_fp),
      reinterpret_cast<uintptr_t*>(&thread_context->arm_ip),
      reinterpret_cast<uintptr_t*>(&thread_context->arm_sp),
      // arm_lr and arm_pc do not require rewriting because they contain
      // addresses of executable code, not addresses in the stack.
  };
#else  // #if defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_32_BITS)
  // Unimplemented for other architectures.
  return {};
#endif
}

}  // namespace base
