// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/test_task_environment.h"

#include "ios/web/public/test/web_task_environment.h"

using base::test::TaskEnvironment;
using web::WebTaskEnvironment;

namespace safe_browsing {

std::unique_ptr<TaskEnvironment> CreateTestTaskEnvironment(
    TaskEnvironment::TimeSource time_source) {
  return std::make_unique<WebTaskEnvironment>(
      WebTaskEnvironment::Options::DEFAULT, time_source);
}

}  // namespace safe_browsing
