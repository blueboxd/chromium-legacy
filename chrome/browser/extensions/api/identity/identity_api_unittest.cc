// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_api.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Tests that all accounts in extensions is enabled when Dice is enabled.
TEST(IdentityApiTest, DiceAllAccountsExtensions) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  IdentityAPI api(&profile);
  EXPECT_FALSE(api.AreExtensionsRestrictedToPrimaryAccount());
  api.Shutdown();
}
#else
TEST(IdentityApiTest, AllAccountsExtensionDisabled) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  IdentityAPI api(&profile);
  EXPECT_TRUE(api.AreExtensionsRestrictedToPrimaryAccount());
  api.Shutdown();
}
#endif

}  // namespace extensions
