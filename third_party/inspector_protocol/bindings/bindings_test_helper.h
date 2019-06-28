// Copyright 2019 The Chromium project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is Chromium specific, to make bindings_test.cc work.  It will work
// in the standalone (upstream) build, as well as in Chromium. In other code
// bases (e.g. v8), a custom file with these two functions and with appropriate
// includes may need to be provided, so it isn't necessarily part of a roll.

#ifndef INSPECTOR_PROTOCOL_BINDINGS_BINDINGS_TEST_HELPER_H_
#define INSPECTOR_PROTOCOL_BINDINGS_BINDINGS_TEST_HELPER_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#endif  // INSPECTOR_PROTOCOL_BINDINGS_BINDINGS_TEST_HELPER_H_
