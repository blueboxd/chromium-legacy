// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/api/management/management_api.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/common/manifest.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"

using extensions::Extension;
using extensions::Manifest;

namespace {

// Find a browser other than |browser|.
Browser* FindOtherBrowser(Browser* browser) {
  Browser* found = NULL;
  for (auto* b : *BrowserList::GetInstance()) {
    if (b == browser)
      continue;
    found = b;
  }
  return found;
}

}  // namespace

class ExtensionManagementApiTest : public extensions::ExtensionApiTest {
 public:
  virtual void LoadExtensions() {
    base::FilePath basedir = test_data_dir_.AppendASCII("management");

    // Load 5 enabled items.
    LoadNamedExtension(basedir, "enabled_extension");
    LoadNamedExtension(basedir, "enabled_app");
    LoadNamedExtension(basedir, "description");
    LoadNamedExtension(basedir, "permissions");
    LoadNamedExtension(basedir, "short_name");

    // Load 2 disabled items.
    LoadNamedExtension(basedir, "disabled_extension");
    DisableExtension(extension_ids_["disabled_extension"]);
    LoadNamedExtension(basedir, "disabled_app");
    DisableExtension(extension_ids_["disabled_app"]);
  }

  // Load an app, and wait for a message from app "management/launch_on_install"
  // indicating that the new app has been launched.
  void LoadAndWaitForLaunch(const std::string& app_path,
                            std::string* out_app_id) {
    ExtensionTestMessageListener launched_app("launched app", false);
    ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(app_path)));

    if (out_app_id)
      *out_app_id = last_loaded_extension_id();

    ASSERT_TRUE(launched_app.WaitUntilSatisfied());
  }

 protected:
  void LoadNamedExtension(const base::FilePath& path,
                          const std::string& name) {
    const Extension* extension = LoadExtension(path.AppendASCII(name));
    ASSERT_TRUE(extension);
    extension_ids_[name] = extension->id();
  }

  void InstallNamedExtension(const base::FilePath& path,
                             const std::string& name,
                             Manifest::Location install_source) {
    const Extension* extension = InstallExtension(path.AppendASCII(name), 1,
                                                  install_source);
    ASSERT_TRUE(extension);
    extension_ids_[name] = extension->id();
  }

  // Maps installed extension names to their IDs.
  std::map<std::string, std::string> extension_ids_;
};

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, Basics) {
  LoadExtensions();

  base::FilePath basedir = test_data_dir_.AppendASCII("management");
  InstallNamedExtension(basedir, "internal_extension", Manifest::INTERNAL);
  InstallNamedExtension(basedir, "external_extension",
                        Manifest::EXTERNAL_PREF);
  InstallNamedExtension(basedir, "admin_extension",
                        Manifest::EXTERNAL_POLICY_DOWNLOAD);
  InstallNamedExtension(basedir, "version_name", Manifest::INTERNAL);

  ASSERT_TRUE(RunExtensionSubtest("management/test", "basics.html"));
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, NoPermission) {
  LoadExtensions();
  ASSERT_TRUE(RunExtensionSubtest("management/no_permission", "test.html"));
}

// Disabled: http://crbug.com/174411
#if defined(OS_WIN)
#define MAYBE_Uninstall DISABLED_Uninstall
#else
#define MAYBE_Uninstall Uninstall
#endif

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, MAYBE_Uninstall) {
  LoadExtensions();
  // Confirmation dialog will be shown for uninstallations except for self.
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);
  ASSERT_TRUE(RunExtensionSubtest("management/test", "uninstall.html"));
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, CreateAppShortcut) {
  LoadExtensions();
  base::FilePath basedir = test_data_dir_.AppendASCII("management");
  LoadNamedExtension(basedir, "packaged_app");

  extensions::ManagementCreateAppShortcutFunction::SetAutoConfirmForTest(true);
  ASSERT_TRUE(RunExtensionSubtest("management/test",
                                  "createAppShortcut.html"));
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, GenerateAppForLink) {
  ASSERT_TRUE(RunExtensionSubtest("management/test",
                                  "generateAppForLink.html"));
}

class InstallReplacementWebAppApiTest : public ExtensionManagementApiTest {
 public:
  InstallReplacementWebAppApiTest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~InstallReplacementWebAppApiTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    ExtensionManagementApiTest::SetUpOnMainThread();
    https_test_server_.ServeFilesFromDirectory(test_data_dir_);
    ASSERT_TRUE(https_test_server_.Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionManagementApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID,
        "odfeghegfpmohakomgihhcnoboeecemb");
  }

  void RunTest(const char* web_app_path,
               const char* background_script,
               bool from_webstore,
               bool whitelisted) {
    static constexpr char kManifest[] =
        R"({
            "name": "Management API Test",
            "version": "0.1",
            "manifest_version": 2,
            "background": { "scripts": ["background.js"] },
            "replacement_web_app": "%s"
          })";

    static constexpr char kPem[] =
        "-----BEGIN PRIVATE KEY-----"
        "MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCs5ycnzJEUSwlN"
        "U7yAuywl8vro4dXew7Eijdd+gYwHAtaQyKxpeJHy09eusWKTfHEaOdqfqssqPMnl"
        "XqoC+Tyt/24xM6rw6uSyAV78DRSAl7AxiyemxTh5P2rzaN4ytJayLpZDzwi38zeZ"
        "QJC4TcSk04bclB2zfLFmMe8W53oxdE8vV6Xa2TPFigR6PV0FcRE40cCPHFhRTDwz"
        "C04b/qW30Ceix2AeLPT4+qsGroq5kLt7zTgvaA+QToKeZNX41snk1w2u/IhOXG+J"
        "0jyZnFU1lgnA9ScMW0laA+Ba2WXB5tLPgyRyyABRRaT5oiJCxRLQc+HFnMdUftGK"
        "D4MKnf+/AgMBAAECggEADJ+/8x7zhMjJwBSaEcgYvBiWi0RZ6i7dkwlKL5lj0Os7"
        "IU0VkYnVFiaze7TF3sDaPTD2Lmw48zeHAjE8NoVeEdIxiHQeSgLMedaxybNmyNDK"
        "c4OWfI2vxuKDe4wvlQIscowGOqM2HsAqUg0tw9chwWsUUKyb0owLI8wHieOSv2OA"
        "w8UlhflqkXLBUc4Mx3iqkIwAyrxQXT/vlA0M8/QvikK/zfeZYZ4f8tg23m3T0fV3"
        "HC4k/Q09MFyUvURVYNpbPHrL83/ZbaHBniEjy+qBX4POO4xrKhow77tr/znB8bsA"
        "T3mRwrEnYoIZmkwxlAdOMNxSYcAKZh4jPWOut0VQ0QKBgQDk341ysCaNzRq7nscR"
        "RzDtpAA+UPcS2vcssXKDRjhsTp31qsUsVsYjTX+O/sv2uyb4HikYiFZOe3iPIfOl"
        "ni7ZfhYFMMIZFjjP0cjQ7C/+ArxGb96DcTbRf7SNTDOLTtZy1jZSgIRek+2vvcr1"
        "a/xPUMCxLEZdUPu+AVhKYHKHOQKBgQDBZVr04r4s5/BygRR3NhFgquI8ffdPHZzC"
        "riEO1X/YOucTs+F+qwTvr25kRozpEjFsZJUibJTDngX9OziatAQdnjt5CtabOXd/"
        "1rSgUadWEvRrcy/aaouCE1J+1unX6Kk5RHmIsK1YP3wC6JrHmqfnEVq9kaoUubTC"
        "WHZfgjQGtwKBgF3B0nD8Bh8quVvIlGXYkwuWll7wzfYUaxMM8gsi1fRQVFcSCMm8"
        "FljZ43pRmH5PdoxH1q/tEeX+oImJ8ASVgz2ncB/aNHkQaF+B4dDsIFDfD/+Ozkls"
        "NHen5+/GGotj1WefpwsvCIqx8LmAd0cIYIihXP53U6/gf+/7Hw8A6YnJAoGAEbhs"
        "xiWEkW7LLGLBck7k9ruRsUNFht1KwNfdtZNAfJqhE8AWuFmJQUEM12lTfgOpvanV"
        "tGrIksgG+nYTsLEv81rNTkD8+wof9fnBYTM6Jvvjo3jReKzsjYWhuHeOw7bQ0quA"
        "i1LM/1oJzeZsUD/OhLClZNtU/0Mo2enrJsMyay8CgYEApCQ8BDMYewQj2MCM92Vw"
        "DWDzqQpfaGIG/eDAeEtdicbfdih3zUWfhEVOpnvf7s7nS8bMVpAo9pGW6sT/s8eX"
        "POGiP9efxb2uHsX06pkAYZm9nddIliWnm0/eDBmSSXPymAZaNYFrex4wxMII20K/"
        "ZX1nuseC+Lx0yzxa/c+iCWg="
        "-----END PRIVATE KEY-----";

    extensions::TestExtensionDir extension_dir;
    extension_dir.WriteManifest(base::StringPrintf(
        kManifest, https_test_server_.GetURL(web_app_path).spec().c_str()));
    extension_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                            background_script);

    base::FilePath crx;
    if (whitelisted)
      crx = extension_dir.PackWithPem(kPem);
    else
      crx = extension_dir.Pack();

    extensions::ResultCatcher catcher;
    if (from_webstore) {
      // |expected_change| is the expected change in the number of installed
      // extensions.
      ASSERT_TRUE(InstallExtensionFromWebstore(crx, 1 /* expected_change */));
    } else {
      ASSERT_TRUE(LoadExtension(crx));
    }

    ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  }

  net::EmbeddedTestServer https_test_server_;
};

IN_PROC_BROWSER_TEST_F(InstallReplacementWebAppApiTest, NotWhitelisted) {
  static constexpr char kBackground[] = R"(
    chrome.test.assertEq(undefined, chrome.management.installReplacementWebApp);
    chrome.test.notifyPass();
  )";

  RunTest("/management/install_replacement_web_app/good_web_app/index.html",
          kBackground, true /* from_webstore */, false /* whitelisted */);
}

IN_PROC_BROWSER_TEST_F(InstallReplacementWebAppApiTest, NotWebstore) {
  static constexpr char kBackground[] = R"(
  chrome.management.installReplacementWebApp(function() {
    chrome.test.assertLastError(
        'Only extensions from the web store can install replacement web apps.');
    chrome.test.notifyPass();
  });)";

  RunTest("/management/install_replacement_web_app/good_web_app/index.html",
          kBackground, false /* from_webstore */, true /* whitelisted */);
}

IN_PROC_BROWSER_TEST_F(InstallReplacementWebAppApiTest, NoGesture) {
  static constexpr char kBackground[] = R"(
  chrome.management.installReplacementWebApp(function() {
    chrome.test.assertLastError(
        'chrome.management.installReplacementWebApp requires a user gesture.');
    chrome.test.notifyPass();
  });)";

  RunTest("/management/install_replacement_web_app/good_web_app/index.html",
          kBackground, true /* from_webstore */, true /* whitelisted */);
}

IN_PROC_BROWSER_TEST_F(InstallReplacementWebAppApiTest, NotInstallableWebApp) {
  static constexpr char kBackground[] =
      R"(chrome.test.runWithUserGesture(function() {
           chrome.management.installReplacementWebApp(function() {
             chrome.test.assertLastError(
                 'Web app is not a valid installable web app.');
             chrome.test.notifyPass();
           });
         });)";

  RunTest("/management/install_replacement_web_app/bad_web_app/index.html",
          kBackground, true /* from_webstore */, true /* whitelisted */);
}

IN_PROC_BROWSER_TEST_F(InstallReplacementWebAppApiTest, InstallableWebApp) {
  static constexpr char kBackground[] =
      R"(chrome.test.runTests([
           function runInstall() {
             chrome.test.runWithUserGesture(function() {
               chrome.management.installReplacementWebApp(function() {
                 chrome.test.assertNoLastError();
                 chrome.test.succeed();
               });
             });
           },
           function runInstallWhenAlreadyInstalled() {
             chrome.test.runWithUserGesture(function() {
               chrome.management.installReplacementWebApp(function() {
                 chrome.test.assertLastError(
                     'Web app is already installed.');
                 chrome.test.succeed();
               });
             });
           }
         ]);)";
  static constexpr char kGoodWebAppURL[] =
      "/management/install_replacement_web_app/good_web_app/index.html";

  chrome::SetAutoAcceptPWAInstallConfirmationForTesting(true);
  const GURL good_web_app_url = https_test_server_.GetURL(kGoodWebAppURL);

  auto* provider =
      web_app::WebAppProviderBase::GetProviderBase(browser()->profile());
  EXPECT_FALSE(provider->registrar().IsInstalled(good_web_app_url));

  RunTest(kGoodWebAppURL, kBackground, true /* from_webstore */,
          true /* whitelisted */);
  EXPECT_TRUE(provider->registrar().IsInstalled(good_web_app_url));
  chrome::SetAutoAcceptPWAInstallConfirmationForTesting(false);
}

// Fails often on Windows dbg bots. http://crbug.com/177163
#if defined(OS_WIN)
#define MAYBE_ManagementPolicyAllowed DISABLED_ManagementPolicyAllowed
#else
#define MAYBE_ManagementPolicyAllowed ManagementPolicyAllowed
#endif  // defined(OS_WIN)
// Tests actions on extensions when no management policy is in place.
IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest,
                       MAYBE_ManagementPolicyAllowed) {
  LoadExtensions();
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(browser()->profile())
          ->extension_service();
  EXPECT_TRUE(service->GetExtensionById(extension_ids_["enabled_extension"],
                                        false));

  // Ensure that all actions are allowed.
  extensions::ExtensionSystem::Get(
      browser()->profile())->management_policy()->UnregisterAllProviders();

  ASSERT_TRUE(RunExtensionSubtest("management/management_policy",
                                  "allowed.html"));
  // The last thing the test does is uninstall the "enabled_extension".
  EXPECT_FALSE(service->GetExtensionById(extension_ids_["enabled_extension"],
                                         true));
}

// Fails often on Windows dbg bots. http://crbug.com/177163
#if defined(OS_WIN)
#define MAYBE_ManagementPolicyProhibited DISABLED_ManagementPolicyProhibited
#else
#define MAYBE_ManagementPolicyProhibited ManagementPolicyProhibited
#endif  // defined(OS_WIN)
// Tests actions on extensions when management policy prohibits those actions.
IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest,
                       MAYBE_ManagementPolicyProhibited) {
  LoadExtensions();
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(browser()->profile())
          ->extension_service();
  EXPECT_TRUE(service->GetExtensionById(extension_ids_["enabled_extension"],
                                        false));

  // Prohibit status changes.
  extensions::ManagementPolicy* policy = extensions::ExtensionSystem::Get(
      browser()->profile())->management_policy();
  policy->UnregisterAllProviders();
  extensions::TestManagementPolicyProvider provider(
    extensions::TestManagementPolicyProvider::PROHIBIT_MODIFY_STATUS);
  policy->RegisterProvider(&provider);
  ASSERT_TRUE(RunExtensionSubtest("management/management_policy",
                                  "prohibited.html"));
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, LaunchPanelApp) {
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(browser()->profile())
          ->extension_service();

  // Load an extension that calls launchApp() on any app that gets
  // installed.
  ExtensionTestMessageListener launcher_loaded("launcher loaded", false);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/launch_on_install")));
  ASSERT_TRUE(launcher_loaded.WaitUntilSatisfied());

  // Load an app with app.launch.container = "panel".
  std::string app_id;
  LoadAndWaitForLaunch("management/launch_app_panel", &app_id);
  ASSERT_FALSE(HasFatalFailure());  // Stop the test if any ASSERT failed.

  // Find the app's browser.  Check that it is a popup.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser = FindOtherBrowser(browser());
  ASSERT_TRUE(app_browser->is_type_popup());
  ASSERT_TRUE(app_browser->is_app());

  // Close the app panel.
  CloseBrowserSynchronously(app_browser);

  // Unload the extension.
  UninstallExtension(app_id);
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_FALSE(service->GetExtensionById(app_id, true));

  // Set a pref indicating that the user wants to launch in a regular tab.
  // This should be ignored, because panel apps always load in a popup.
  extensions::SetLaunchType(browser()->profile(), app_id,
                            extensions::LAUNCH_TYPE_REGULAR);

  // Load the extension again.
  std::string app_id_new;
  LoadAndWaitForLaunch("management/launch_app_panel", &app_id_new);
  ASSERT_FALSE(HasFatalFailure());

  // If the ID changed, then the pref will not apply to the app.
  ASSERT_EQ(app_id, app_id_new);

  // Find the app's browser.  Apps that should load in a panel ignore
  // prefs, so we should still see the launch in a popup.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  app_browser = FindOtherBrowser(browser());
  ASSERT_TRUE(app_browser->is_type_popup());
  ASSERT_TRUE(app_browser->is_app());
}

// Disabled: crbug.com/230165, crbug.com/915339, crbug.com/979399
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
#define MAYBE_LaunchTabApp DISABLED_LaunchTabApp
#else
#define MAYBE_LaunchTabApp LaunchTabApp
#endif

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, MAYBE_LaunchTabApp) {
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(browser()->profile())
          ->extension_service();

  // Load an extension that calls launchApp() on any app that gets
  // installed.
  ExtensionTestMessageListener launcher_loaded("launcher loaded", false);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/launch_on_install")));
  ASSERT_TRUE(launcher_loaded.WaitUntilSatisfied());

  // Code below assumes that the test starts with a single browser window
  // hosting one tab.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  // Load an app with app.launch.container = "tab".
  std::string app_id;
  LoadAndWaitForLaunch("management/launch_app_tab", &app_id);
  ASSERT_FALSE(HasFatalFailure());

  // Check that the app opened in a new tab of the existing browser.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  // Unload the extension.
  UninstallExtension(app_id);
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_FALSE(service->GetExtensionById(app_id, true));

  // Set a pref indicating that the user wants to launch in a window.
  extensions::SetLaunchType(browser()->profile(), app_id,
                            extensions::LAUNCH_TYPE_WINDOW);

  std::string app_id_new;
  LoadAndWaitForLaunch("management/launch_app_tab", &app_id_new);
  ASSERT_FALSE(HasFatalFailure());

  // If the ID changed, then the pref will not apply to the app.
  ASSERT_EQ(app_id, app_id_new);

  // Find the app's browser.  Opening in a new window will create
  // a new browser.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser = FindOtherBrowser(browser());
  ASSERT_TRUE(app_browser->is_app());
}

// Flaky on MacOS: crbug.com/915339
#if defined(OS_MACOSX)
#define MAYBE_LaunchType DISABLED_LaunchType
#else
#define MAYBE_LaunchType LaunchType
#endif
IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, MAYBE_LaunchType) {
  LoadExtensions();
  base::FilePath basedir = test_data_dir_.AppendASCII("management");
  LoadNamedExtension(basedir, "packaged_app");

  ASSERT_TRUE(RunExtensionSubtest("management/test", "launchType.html"));
}
