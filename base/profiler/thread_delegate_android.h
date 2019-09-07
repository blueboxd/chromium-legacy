// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROFILER_THREAD_DELEGATE_ANDROID_H_
#define BASE_PROFILER_THREAD_DELEGATE_ANDROID_H_

#include "base/base_export.h"
#include "base/profiler/thread_delegate.h"

namespace base {

// Platform- and thread-specific implementation in support of stack sampling on
// Android.
//
// TODO(https://crbug.com/988579): Implement this class.
class BASE_EXPORT ThreadDelegateAndroid : public ThreadDelegate {
 public:
  ThreadDelegateAndroid() = default;

  ThreadDelegateAndroid(const ThreadDelegateAndroid&) = delete;
  ThreadDelegateAndroid& operator=(const ThreadDelegateAndroid&) = delete;

  // ThreadDelegate
  uintptr_t GetStackBaseAddress() const override;
  std::vector<uintptr_t*> GetRegistersToRewrite(
      RegisterContext* thread_context) override;
};

}  // namespace base

#endif  // BASE_PROFILER_THREAD_DELEGATE_ANDROID_H_
