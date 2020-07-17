// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/machine_learning/machine_learning_service.h"

#include "base/macros.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace machine_learning {

class MachineLearningServiceTest : public ::testing::Test {
 public:
  MachineLearningServiceTest() = default;
  ~MachineLearningServiceTest() override = default;
};

TEST_F(MachineLearningServiceTest, InstantiateService) {
  MachineLearningService service{mojo::NullReceiver()};
}

// TODO(crbug/1102425): Add tests for MachineLearningService.

}  // namespace machine_learning
