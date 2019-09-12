// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/android/features/test_dummy/public/test_dummy.h"

extern "C" {

__attribute__((visibility("default"))) int TestDummyEntrypoint() {
  return test_dummy::TestDummy();
}

}  // extern "C"
