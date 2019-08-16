// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"

#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_api.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

using ::testing::ElementsAre;
using ::testing::Field;

TEST(LeakDetectionRequestUtils, PrepareSingleLeakRequestData) {
  base::test::ScopedTaskEnvironment task_env;
  base::MockCallback<SingleLeakRequestDataCallback> callback;

  PrepareSingleLeakRequestData("jonsnow", "1234", callback.Get());
  EXPECT_CALL(
      callback,
      Run(AllOf(
          Field(&LookupSingleLeakData::username_hash_prefix,
                ElementsAre(61, 112, -45)),
          Field(&LookupSingleLeakData::encrypted_payload, testing::Ne("")),
          Field(&LookupSingleLeakData::encryption_key, testing::Ne("")))));
  task_env.RunUntilIdle();
}

}  // namespace password_manager
