// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MEDIA_APP_UI_TEST_MEDIA_APP_UI_BROWSERTEST_H_
#define CHROMEOS_COMPONENTS_MEDIA_APP_UI_TEST_MEDIA_APP_UI_BROWSERTEST_H_

#include <string>

#include "chromeos/components/web_applications/test/sandboxed_web_ui_test_base.h"

class MediaAppUiBrowserTest : public SandboxedWebUiAppTestBase {
 public:
  MediaAppUiBrowserTest();
  ~MediaAppUiBrowserTest() override;

  MediaAppUiBrowserTest(const MediaAppUiBrowserTest&) = delete;
  MediaAppUiBrowserTest& operator=(const MediaAppUiBrowserTest&) = delete;

  // Returns the contents of the JavaScript library used to help test the
  // sandboxed frame.
  static std::string AppJsTestLibrary();
};

#endif  // CHROMEOS_COMPONENTS_MEDIA_APP_UI_TEST_MEDIA_APP_UI_BROWSERTEST_H_
