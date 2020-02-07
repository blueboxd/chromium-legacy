// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/settings/settings_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/url_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

typedef InProcessBrowserTest SettingsUITest;

using ui_test_utils::NavigateToURL;

namespace {

class MockHatsService : public HatsService {
 public:
  explicit MockHatsService(Profile* profile) : HatsService(profile) {}
  ~MockHatsService() override = default;

  MOCK_METHOD(void, LaunchSurvey, (const std::string& trigger), (override));
};

std::unique_ptr<KeyedService> BuildMockHatsService(
    content::BrowserContext* context) {
  return std::make_unique<MockHatsService>(static_cast<Profile*>(context));
}

}  // namespace

IN_PROC_BROWSER_TEST_F(SettingsUITest, ViewSourceDoesntCrash) {
  NavigateToURL(browser(),
                GURL(content::kViewSourceScheme + std::string(":") +
                     chrome::kChromeUISettingsURL + std::string("strings.js")));
}

// Catch lifetime issues in message handlers. There was previously a problem
// with PrefMember calling Init again after Destroy.
IN_PROC_BROWSER_TEST_F(SettingsUITest, ToggleJavaScript) {
  NavigateToURL(browser(), GURL(chrome::kChromeUISettingsURL));

  const auto& handlers = *browser()
                              ->tab_strip_model()
                              ->GetActiveWebContents()
                              ->GetWebUI()
                              ->GetHandlersForTesting();

  for (const std::unique_ptr<content::WebUIMessageHandler>& handler :
       handlers) {
    handler->AllowJavascriptForTesting();
    handler->DisallowJavascript();
    handler->AllowJavascriptForTesting();
  }
}

IN_PROC_BROWSER_TEST_F(SettingsUITest, TriggerHappinessTrackingSurveys) {
  MockHatsService* mock_hats_service_ = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          browser()->profile(), base::BindRepeating(&BuildMockHatsService)));
  settings::SettingsUI::SetHatsTimeoutForTesting(0);
  EXPECT_CALL(*mock_hats_service_, LaunchSurvey(kHatsSurveyTriggerSettings));
  NavigateToURL(browser(), GURL(chrome::kChromeUISettingsURL));
  base::RunLoop().RunUntilIdle();
}
