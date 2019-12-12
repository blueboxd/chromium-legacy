// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/bind_test_util.h"
#include "chrome/browser/chromeos/app_mode/app_session.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class AppSessionTest : public testing::Test {
 public:
  AppSessionTest() = default;
  AppSessionTest(const AppSessionTest&) = delete;
  AppSessionTest& operator=(const AppSessionTest&) = delete;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(AppSessionTest, WebKioskTracksBrowserCreation) {
  auto app_session = std::make_unique<AppSession>();
  TestingProfile profile;

  Browser::CreateParams params(&profile, true);
  auto app_browser = CreateBrowserWithTestWindowForParams(&params);

  app_session->InitForWebKiosk(app_browser.get());

  Browser::CreateParams another_params(&profile, true);
  auto another_browser = CreateBrowserWithTestWindowForParams(&another_params);

  base::RunLoop loop;
  static_cast<TestBrowserWindow*>(another_params.window)
      ->SetCloseCallback(
          base::BindLambdaForTesting([&loop]() { loop.Quit(); }));
  loop.Run();

  bool chrome_closed = false;
  app_session->SetAttemptUserExitForTesting(
      base::BindLambdaForTesting([&chrome_closed]() { chrome_closed = true; }));

  app_browser.reset();
  ASSERT_TRUE(chrome_closed);
}

}  // namespace chromeos
