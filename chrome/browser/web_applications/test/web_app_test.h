// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_H_

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

enum class ProviderType { kBookmarkApps, kWebApps };

std::string ProviderTypeParamToString(
    const ::testing::TestParamInfo<ProviderType>& provider_type);

}  // namespace web_app

// Consider to implement web app specific test harness independent of
// RenderViewHost.
using WebAppTest = ChromeRenderViewHostTestHarness;

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_H_
