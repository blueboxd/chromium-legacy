// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "apps/launcher.h"
#include "apps/test/app_window_waiter.h"
#include "ash/public/cpp/ash_features.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/chromeos/kiosk_next/kiosk_next_browser_factory.h"
#include "chrome/browser/chromeos/login/mixin_based_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/session/user_session_manager_test_api.h"
#include "chrome/browser/chromeos/login/test/embedded_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/login/auth/user_context.h"
#include "components/account_id/account_id.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "net/dns/mock_host_resolver.h"
#include "url/gurl.h"

namespace chromeos {
namespace kiosk_next_home {

namespace {

const char* kTestUserGivenName = "Test User";
const char* kTestUserDisplayName = "Test User Full Name";
const char* kTestUser = "test-user@gmail.com";
const char* kTestUserGaiaId = "1234567890";

// Helper class to wait until WebContents finishes loading.
class WebContentsLoadFinishedWaiter : public content::WebContentsObserver {
 public:
  explicit WebContentsLoadFinishedWaiter(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ~WebContentsLoadFinishedWaiter() override = default;

  void Wait() {
    if (!web_contents()->IsLoading())
      return;

    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

 private:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& url) override {
    if (run_loop_)
      run_loop_->Quit();
  }

  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsLoadFinishedWaiter);
};

const Browser* GetBrowserForWebsite(const GURL& url) {
  BrowserList* list = BrowserList::GetInstance();
  for (const Browser* browser : *list) {
    TabStripModel* tab_strip_model = browser->tab_strip_model();
    for (int tab_index = 0; tab_index < tab_strip_model->count(); tab_index++) {
      content::WebContents* web_contents =
          tab_strip_model->GetWebContentsAt(tab_index);
      if (web_contents && (web_contents->GetURL() == url))
        return browser;
    }
  }
  return nullptr;
}

}  // namespace

// Integration test for Kiosk Next Home app, exercising the Chrome <-> Mojo <->
// JavaScript bridge interaction. This ensures the services backing Kiosk Next
// Home are accessible and that the generated Mojo JS code is working.
class KioskNextHomeBrowserTest : public MixinBasedInProcessBrowserTest {
 public:
  KioskNextHomeBrowserTest() { set_exit_when_last_browser_closes(false); }

  void SetUp() override {
    feature_list_.InitAndEnableFeature(ash::features::kKioskNextShell);
    MixinBasedInProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    // Enable all component extensions.
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();

    test::UserSessionManagerTestApi session_manager_test_api(
        UserSessionManager::GetInstance());
    session_manager_test_api.SetShouldObtainTokenHandleInTests(false);
    login_manager_mixin_.LoginAndWaitForActiveSession(GetUserContext());

    MixinBasedInProcessBrowserTest::SetUpOnMainThread();

    LaunchKioskNextHome();
  }

 protected:
  virtual UserContext GetUserContext() const {
    return LoginManagerMixin::CreateDefaultUserContext(test_user_);
  }

  void RunJS(const std::string& js_script) {
    EXPECT_TRUE(content::ExecuteScript(app_render_frame_host_, js_script));
  }

  // Runs JavaScript code in the app's render frame host. To extract the string
  // result, the code must call 'domAutomationController.send(result)' with the
  // desired result, otherwise it will hang until timeout.
  std::string RunJSAndGetStringResult(const std::string& js_script) {
    std::string result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(app_render_frame_host_,
                                                       js_script, &result));
    return result;
  }

  // Similar to RunJSAndGetStringResult, but for a double result.
  double RunJSAndGetDoubleResult(const std::string& js_script) {
    double result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractDouble(app_render_frame_host_,
                                                       js_script, &result));
    return result;
  }

  LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId(kTestUser, kTestUserGaiaId)};

 private:
  void LaunchKioskNextHome() {
    Profile* profile = ProfileManager::GetActiveUserProfile();
    // Get reference to Kiosk Next Home app.
    const extensions::Extension* app =
        extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
            extension_misc::kKioskNextHomeAppId);
    ASSERT_TRUE(app);

    // Launch app and wait for its window.
    apps::LaunchPlatformApp(
        profile, app, extensions::AppLaunchSource::SOURCE_CHROME_INTERNAL);
    apps::AppWindowWaiter app_waiter(
        extensions::AppWindowRegistry::Get(profile),
        extension_misc::kKioskNextHomeAppId);
    extensions::AppWindow* window =
        app_waiter.WaitForShownWithTimeout(TestTimeouts::action_timeout());
    ASSERT_TRUE(window);

    // Wait for web contents to fully load.
    WebContentsLoadFinishedWaiter web_contents_waiter(
        window->app_window_contents_for_test()->GetWebContents());
    web_contents_waiter.Wait();
    app_render_frame_host_ = window->app_window_contents_for_test()
                                 ->GetWebContents()
                                 ->GetMainFrame();
    ASSERT_TRUE(app_render_frame_host_);
  }

  base::test::ScopedFeatureList feature_list_;
  LoginManagerMixin login_manager_mixin_{&mixin_host_, {test_user_}};
  EmbeddedTestServerSetupMixin embedded_test_server_{&mixin_host_,
                                                     embedded_test_server()};
  content::RenderFrameHost* app_render_frame_host_;

  DISALLOW_COPY_AND_ASSIGN(KioskNextHomeBrowserTest);
};

IN_PROC_BROWSER_TEST_F(KioskNextHomeBrowserTest, LaunchIntent) {
  // Intents are not allowed by default.
  std::string error_message = RunJSAndGetStringResult(
      R"(kioskNextHome.getChromeOsBridge().launchIntent('test.intent')
           .catch(error => domAutomationController.send(error)))");
  EXPECT_EQ("Intent not allowed.", error_message);
}

IN_PROC_BROWSER_TEST_F(KioskNextHomeBrowserTest, GetApps) {
  // Perform checks on apps on JS side and verify a summary of possible errors,
  // which is simpler than piping apps and representing them in C++ code.
  std::string error_message = RunJSAndGetStringResult(
      R"(kioskNextHome.getChromeOsBridge().getApps().then(apps => {
           const errorMessages = [];
           if (!apps) {
             errorMessages.push('No apps returned.');
           }
           apps.forEach(app => {
             if (!app.appId) {
               errorMessages.push('Missing app id.');
             }
             if (!app.displayName) {
               errorMessages.push('Missing app display name.');
             }
             if (!app.thumbnailImage ||
                 !app.thumbnailImage.startsWith('chrome://app-icon')) {
               errorMessages.push('Invalid or missing app thumbnail: ' +
                                  app.thumbnailImage);
             }
             if (app.readiness == kioskNextHome.AppReadiness.UNKNOWN) {
               errorMessages.push(
                 'Got unknown readiness for app id ' + app.appId);
             }
             if (app.type == kioskNextHome.AppType.UNKNOWN) {
               errorMessages.push(
                 'Got unknown type for app id ' + app.appId);
             }
           });
           domAutomationController.send(errorMessages.join('\n'));
         }))");
  EXPECT_TRUE(error_message.empty())
      << "Found errors in GetApps response: " << std::endl
      << error_message;

  // Get a semicolon separated list of app ids.
  std::string app_ids = RunJSAndGetStringResult(
      R"(kioskNextHome.getChromeOsBridge().getApps().then(apps => {
           const appIds = [];
           apps.forEach(app => appIds.push(app.appId));
           domAutomationController.send(appIds.join(';'));
         }))");
  // Check that Files Manager's id is present.
  EXPECT_TRUE(app_ids.find(extension_misc::kFilesManagerAppId) !=
              std::string::npos);
  // Check that Kiosk Next Home's own id is *not* present.
  EXPECT_TRUE(app_ids.find(extension_misc::kKioskNextHomeAppId) ==
              std::string::npos);
}

IN_PROC_BROWSER_TEST_F(KioskNextHomeBrowserTest, LaunchApp) {
  RunJS(content::JsReplace("kioskNextHome.getChromeOsBridge().launchApp($1)",
                           extension_misc::kFilesManagerAppId));
  apps::AppWindowWaiter files_manager_waiter(
      extensions::AppWindowRegistry::Get(
          ProfileManager::GetActiveUserProfile()),
      extension_misc::kFilesManagerAppId);
  EXPECT_TRUE(files_manager_waiter.WaitForShownWithTimeout(
      TestTimeouts::action_timeout()));
}

IN_PROC_BROWSER_TEST_F(KioskNextHomeBrowserTest, LaunchKioskNextBrowser) {
  const std::string kUrl = "https://www.url1.com";
  RunJS(content::JsReplace(
      "kioskNextHome.getChromeOsBridge().launchKioskNextWebsite({url : $1})",
      kUrl));
  Browser* browser =
      KioskNextBrowserList::Get().GetBrowserForWebsite(GURL(kUrl));
  EXPECT_NE(browser, nullptr);
}

IN_PROC_BROWSER_TEST_F(KioskNextHomeBrowserTest, LaunchWebsite) {
  const std::string kUrl = "https://www.url1.com";
  RunJS(content::JsReplace(
      "kioskNextHome.getChromeOsBridge().launchWebsite({url : $1})", kUrl));
  const Browser* browser =
      KioskNextBrowserList::Get().GetBrowserForWebsite(GURL(kUrl));
  EXPECT_EQ(browser, nullptr);
  EXPECT_NE(GetBrowserForWebsite(GURL(kUrl)), nullptr);
}
// Fixture variant that sets up additional user information for
// GetUser(Given|Display)Name methods.
class KioskNextHomeUserInfoBrowserTest : public KioskNextHomeBrowserTest {
 public:
  KioskNextHomeUserInfoBrowserTest() = default;

  void SetUpOnMainThread() override {
    user_manager::UserManager::Get()->UpdateUserAccountData(
        test_user_.account_id, user_manager::UserManager::UserAccountData(
                                   base::ASCIIToUTF16(kTestUserDisplayName),
                                   base::ASCIIToUTF16(kTestUserGivenName),
                                   std::string() /* locale */));

    KioskNextHomeBrowserTest::SetUpOnMainThread();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(KioskNextHomeUserInfoBrowserTest);
};

IN_PROC_BROWSER_TEST_F(KioskNextHomeUserInfoBrowserTest, GetUserGivenName) {
  std::string given_name = RunJSAndGetStringResult(
      R"(kioskNextHome.getChromeOsBridge().getUserGivenName()
           .then(name => domAutomationController.send(name)))");
  EXPECT_EQ(kTestUserGivenName, given_name);
}

IN_PROC_BROWSER_TEST_F(KioskNextHomeUserInfoBrowserTest, GetUserDisplayName) {
  std::string display_name = RunJSAndGetStringResult(
      R"(kioskNextHome.getChromeOsBridge().getUserDisplayName()
           .then(name => domAutomationController.send(name)))");
  EXPECT_EQ(kTestUserDisplayName, display_name);
}

// Fixture variant that sets up a fake refresh token needed to issue access
// tokens and access account info via IdentityAccessor.
class KioskNextHomeRefreshTokenBrowserTest : public KioskNextHomeBrowserTest {
 public:
  KioskNextHomeRefreshTokenBrowserTest() = default;

  void SetUpOnMainThread() override {
    fake_gaia_.SetupFakeGaiaForLogin(kTestUser, kTestUserGaiaId,
                                     FakeGaiaMixin::kFakeRefreshToken);

    KioskNextHomeBrowserTest::SetUpOnMainThread();
  }

  UserContext GetUserContext() const override {
    UserContext user_context = KioskNextHomeBrowserTest::GetUserContext();
    user_context.SetRefreshToken(FakeGaiaMixin::kFakeRefreshToken);
    return user_context;
  }

 private:
  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};

  DISALLOW_COPY_AND_ASSIGN(KioskNextHomeRefreshTokenBrowserTest);
};

IN_PROC_BROWSER_TEST_F(KioskNextHomeRefreshTokenBrowserTest, GetAccessToken) {
  std::string access_token = RunJSAndGetStringResult(
      R"(kioskNextHome.getChromeOsBridge().getAccessToken(['fake_scope'])
           .then(token => domAutomationController.send(token)))");
  EXPECT_EQ(access_token, FakeGaiaMixin::kFakeAllScopeAccessToken);
}

IN_PROC_BROWSER_TEST_F(KioskNextHomeRefreshTokenBrowserTest,
                       FetchAccessTokenExpiration) {
  // Access token fetchers take a 10% margin from token expirations before
  // sending them to avoid returning an expired token.
  int expected_expiration = FakeGaiaMixin::kFakeAccessTokenExpiration * 9 / 10;

  double raw_expiration_time = RunJSAndGetDoubleResult(
      R"(kioskNextHome.getChromeOsBridge().fetchAccessToken(['fake_scope'])
           .then(tokenInfo => {
                   domAutomationController.send(tokenInfo.expirationTime);
                 }))");
  base::Time expiration_time = base::Time::FromJsTime(raw_expiration_time);
  base::TimeDelta actual_expiration = expiration_time - base::Time::Now();

  // We give a 5 second margin of error for the token expiration. In this test
  // we are mostly interested if the expiration reaches the Kiosk Next Home, not
  // in per second precision.
  EXPECT_NEAR(actual_expiration.InSeconds(), expected_expiration, 5);
}

IN_PROC_BROWSER_TEST_F(KioskNextHomeRefreshTokenBrowserTest, GetAccountId) {
  std::string id = RunJSAndGetStringResult(
      R"(kioskNextHome.getChromeOsBridge().getAccountId()
           .then(id => domAutomationController.send(id)))");
  EXPECT_EQ(kTestUserGaiaId, id);
}

}  // namespace kiosk_next_home
}  // namespace chromeos
