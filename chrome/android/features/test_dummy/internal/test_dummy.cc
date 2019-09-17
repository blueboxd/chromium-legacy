// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/android/features/test_dummy/public/test_dummy.h"

#include "base/logging.h"

namespace test_dummy {

int TestDummy() {
  // Log something to utilize base library code. This is necessary to ensure
  // that calls to base code are linked properly.
  LOG(WARNING) << "Running test dummy native library.";
  return 123;
}

}  // namespace test_dummy
