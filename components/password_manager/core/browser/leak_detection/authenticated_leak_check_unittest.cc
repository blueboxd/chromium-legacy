// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/authenticated_leak_check.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

TEST(AuthenticatedLeakCheck, Create) {
  AuthenticatedLeakCheck check;
}

}  // namespace password_manager
