// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_COMMON_TEST_TASK_ENVIRONMENT_H_
#define COMPONENTS_SAFE_BROWSING_CORE_COMMON_TEST_TASK_ENVIRONMENT_H_

#include <memory>

#include "base/test/task_environment.h"

namespace safe_browsing {

// Creates a TaskEnvironment suitable for the current platform (content/ or
// ios/), allowing usage of the UI and IO threads.
std::unique_ptr<base::test::TaskEnvironment> CreateTestTaskEnvironment(
    base::test::TaskEnvironment::TimeSource time_source =
        base::test::TaskEnvironment::TimeSource::DEFAULT);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_COMMON_TEST_TASK_ENVIRONMENT_H_
