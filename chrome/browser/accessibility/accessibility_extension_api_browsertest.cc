// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "ui/base/ui_base_features.h"

namespace extensions {

using AccessibilityPrivateApiTest = ExtensionApiTest;

IN_PROC_BROWSER_TEST_F(AccessibilityPrivateApiTest, SendSyntheticKeyEvent) {
  ASSERT_TRUE(RunExtensionSubtest("accessibility_private/",
                                  "send_synthetic_key_event.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AccessibilityPrivateApiTest, GetDisplayLanguageTest) {
  ASSERT_TRUE(
      RunExtensionSubtest("accessibility_private/", "display_language.html"))
      << message_;
}

}  // namespace extensions
