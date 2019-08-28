// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_SAFETY_TIP_TEST_UTILS_H_
#define CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_SAFETY_TIP_TEST_UTILS_H_

#include <string>

// Sets the patterns to trigger a bad-reputation Safety Tip for tests.
void SetSafetyTipBadRepPatterns(std::vector<std::string> pattern);

#endif  // CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_SAFETY_TIP_TEST_UTILS_H_
