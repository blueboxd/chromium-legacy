// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/application_lifetime.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/testing_pref_service.h"

using ApplicationLifetimeTest = BrowserWithTestWindowTest;

TEST_F(ApplicationLifetimeTest, AttemptRestart) {
  ASSERT_TRUE(g_browser_process);
  TestingPrefServiceSimple* testing_pref_service =
      profile_manager()->local_state()->Get();

  EXPECT_FALSE(testing_pref_service->GetBoolean(prefs::kWasRestarted));
  chrome::AttemptRestart();
  EXPECT_TRUE(testing_pref_service->GetBoolean(prefs::kWasRestarted));

  // Cancel the effects of us calling chrome::AttemptRestart. Otherwise tests
  // ran after this one will fail.
  browser_shutdown::SetTryingToQuit(false);
}

TEST_F(ApplicationLifetimeTest, AttemptRestartWithIncognitoAfterRegular) {
  ASSERT_TRUE(g_browser_process);
  base::CommandLine& old_cl(*base::CommandLine::ForCurrentProcess());
  TestingPrefServiceSimple* testing_pref_service =
      profile_manager()->local_state()->Get();
  EXPECT_FALSE(testing_pref_service->GetBoolean(prefs::kWasRestarted));

  // Create a new incognito browser.
  TestingProfile::Builder profile_builder;
  std::unique_ptr<TestingProfile> test_profile = profile_builder.Build();
  Browser::CreateParams params{test_profile->GetOffTheRecordProfile(), false};
  std::unique_ptr<BrowserWindow> test_window(CreateBrowserWindow());
  params.window = test_window.get();
  std::unique_ptr<Browser> incognito_browser(Browser::Create(params));

  EXPECT_TRUE(incognito_browser);
  EXPECT_TRUE(incognito_browser->profile()->IsIncognitoProfile());
  BrowserList::SetLastActive(incognito_browser.get());

  // When the first browser was created it was created in regular
  // mode, so the command line arguments for it shouldn't have the incongito
  // switch.
  EXPECT_FALSE(old_cl.HasSwitch(switches::kIncognito));
  // We will now attempt restart, prior to (crbug.com/999085)
  // the new session after restart defaulted to the browser type
  // of the last session. We now restart to the browser type of
  // the user who invoked restart, which is emulated here,
  // via the method BrowserList::SetLastActive().
  chrome::AttemptRestart();
  // Since, the last active browser was of type Incognito, we now
  // expect to have the kIncognito switch added to the old_cl cmd line
  // arguments, the old_cl arguments are then used to create the
  // new cmd line session post restart.
  EXPECT_TRUE(old_cl.HasSwitch(switches::kIncognito));
  EXPECT_TRUE(testing_pref_service->GetBoolean(prefs::kWasRestarted));

  // Cancel the effects of us calling chrome::AttemptRestart. Otherwise tests
  // ran after this one will fail.
  browser_shutdown::SetTryingToQuit(false);

  // We perform the run loop before to avoid ASAN failures which access
  // the Testing profile after the test is run.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ApplicationLifetimeTest, AttemptRestartWithRegularAfterIncognito) {
  ASSERT_TRUE(g_browser_process);
  base::CommandLine& old_cl(*base::CommandLine::ForCurrentProcess());
  TestingPrefServiceSimple* testing_pref_service =
      profile_manager()->local_state()->Get();
  EXPECT_FALSE(testing_pref_service->GetBoolean(prefs::kWasRestarted));

  // Create a new incognito browser.
  TestingProfile::Builder profile_builder;
  std::unique_ptr<TestingProfile> test_profile = profile_builder.Build();
  Browser::CreateParams params{test_profile->GetOffTheRecordProfile(), false};
  std::unique_ptr<BrowserWindow> test_window(CreateBrowserWindow());
  params.window = test_window.get();
  std::unique_ptr<Browser> incognito_browser(Browser::Create(params));
  EXPECT_TRUE(incognito_browser);
  EXPECT_TRUE(incognito_browser->profile()->IsIncognitoProfile());

  // Since the original browser that is created in the Setup() phase
  // is always a regular browser, we emulate the incognito part
  // by apppending |kIncongito| switch to its command line args.
  old_cl.AppendSwitch(switches::kIncognito);

  // Make the regular browser last active.
  BrowserList::SetLastActive(browser());

  EXPECT_TRUE(old_cl.HasSwitch(switches::kIncognito));
  chrome::AttemptRestart();
  EXPECT_FALSE(old_cl.HasSwitch(switches::kIncognito));
  EXPECT_TRUE(testing_pref_service->GetBoolean(prefs::kWasRestarted));

  // Cancel the effects of us calling chrome::AttemptRestart. Otherwise tests
  // ran after this one will fail.
  browser_shutdown::SetTryingToQuit(false);

  // We perform the run loop before to avoid ASAN failures which access
  // the Testing profile after the test is run.
  base::RunLoop().RunUntilIdle();
}
