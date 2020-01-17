// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/test_task_environment.h"

#include "content/public/test/browser_task_environment.h"

using base::test::TaskEnvironment;
using content::BrowserTaskEnvironment;

namespace safe_browsing {

std::unique_ptr<TaskEnvironment> CreateTestTaskEnvironment(
    TaskEnvironment::TimeSource time_source) {
  return std::make_unique<BrowserTaskEnvironment>(time_source);
}

}  // namespace safe_browsing
