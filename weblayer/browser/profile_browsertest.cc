// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/test/weblayer_browser_test.h"

namespace weblayer {

using ProfileBrowsertest = WebLayerBrowserTest;

// TODO(crbug.com/654704): Android does not support PRE_ tests.
#if !defined(OS_ANDROID)

// UKM enabling via Profile persists across restarts.
IN_PROC_BROWSER_TEST_F(ProfileBrowsertest, PRE_PersistUKM) {
  GetProfile()->SetBooleanSetting(SettingType::UKM_ENABLED, true);
}

IN_PROC_BROWSER_TEST_F(ProfileBrowsertest, PersistUKM) {
  ASSERT_TRUE(GetProfile()->GetBooleanSetting(SettingType::UKM_ENABLED));
}

#endif  // !defined(OS_ANDROID)

}  // namespace weblayer
