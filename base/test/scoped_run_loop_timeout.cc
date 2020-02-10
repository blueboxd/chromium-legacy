// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_run_loop_timeout.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace test {

namespace {
bool g_add_gtest_failure_on_timeout = false;
}

ScopedRunLoopTimeout::ScopedRunLoopTimeout(TimeDelta timeout)
    : nested_timeout_(RunLoop::GetTimeoutForCurrentThread()) {
  DCHECK_GT(timeout, TimeDelta());
  run_timeout_.timeout = timeout;

  if (g_add_gtest_failure_on_timeout) {
    run_timeout_.on_timeout = base::BindRepeating(
        []() { GTEST_FAIL() << "RunLoop::Run() timed out."; });
  } else {
    run_timeout_.on_timeout = base::BindRepeating(
        []() { LOG(FATAL) << "RunLoop::Run() timed out."; });
  }

  RunLoop::SetTimeoutForCurrentThread(&run_timeout_);
}

ScopedRunLoopTimeout::~ScopedRunLoopTimeout() {
  RunLoop::SetTimeoutForCurrentThread(nested_timeout_);
}

// static
bool ScopedRunLoopTimeout::ExistsForCurrentThread() {
  return RunLoop::GetTimeoutForCurrentThread() != nullptr;
}

// static
void ScopedRunLoopTimeout::SetAddGTestFailureOnTimeout() {
  g_add_gtest_failure_on_timeout = true;
}

// static
const RunLoop::RunLoopTimeout*
ScopedRunLoopTimeout::GetTimeoutForCurrentThread() {
  return RunLoop::GetTimeoutForCurrentThread();
}

ScopedDisableRunLoopTimeout::ScopedDisableRunLoopTimeout()
    : nested_timeout_(RunLoop::GetTimeoutForCurrentThread()) {
  RunLoop::SetTimeoutForCurrentThread(nullptr);
}

ScopedDisableRunLoopTimeout::~ScopedDisableRunLoopTimeout() {
  RunLoop::SetTimeoutForCurrentThread(nested_timeout_);
}

}  // namespace test
}  // namespace base
