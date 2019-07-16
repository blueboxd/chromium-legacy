// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"

#include <memory>
#include <vector>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/instant_test_base.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/test_native_theme.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#include "extensions/common/extension_urls.h"
#include "url/url_constants.h"
#endif

namespace content {

namespace {

enum class NetworkServiceState {
  kDisabled,
  kEnabled,
};

}  // namespace

// Use a test class with SetUpCommandLine to ensure the flag is sent to the
// first renderer process.
class ChromeContentBrowserClientBrowserTest : public InProcessBrowserTest {
 public:
  ChromeContentBrowserClientBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeContentBrowserClientBrowserTest);
};

// Test that a basic navigation works in --site-per-process mode.  This prevents
// regressions when that mode calls out into the ChromeContentBrowserClient,
// such as http://crbug.com/164223.
IN_PROC_BROWSER_TEST_F(ChromeContentBrowserClientBrowserTest,
                       SitePerProcessNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/title1.html"));

  ui_test_utils::NavigateToURL(browser(), url);
  NavigationEntry* entry = browser()
                               ->tab_strip_model()
                               ->GetWebContentsAt(0)
                               ->GetController()
                               .GetLastCommittedEntry();

  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(url, entry->GetURL());
  EXPECT_EQ(url, entry->GetVirtualURL());
}

class ChromeContentBrowserClientPopupsTest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Required setup for kAllowPopupsDuringPageUnload switch
    // as its being checked (whether its going to be enabled or not)
    // only if the process type is renderer process.
    command_line_.AppendSwitchASCII(switches::kProcessType,
                                    switches::kRendererProcess);
  }
  void SetUpOnMainThread() override {
    kChildProcessId = browser()
                          ->tab_strip_model()
                          ->GetActiveWebContents()
                          ->GetMainFrame()
                          ->GetProcess()
                          ->GetID();
  }
  ChromeContentBrowserClientPopupsTest()
      : command_line_(base::CommandLine::NO_PROGRAM) {}

  void AppendContentBrowserClientSwitches() {
    client_.AppendExtraCommandLineSwitches(&command_line_, kChildProcessId);
  }

  const base::CommandLine& command_line() const { return command_line_; }

 private:
  ChromeContentBrowserClient client_;
  base::CommandLine command_line_;
  int kChildProcessId;
};

IN_PROC_BROWSER_TEST_F(ChromeContentBrowserClientPopupsTest,
                       AllowPopupsDuringPageUnload) {
  // Verify that the switch is included only when the
  // pref AllowPopupsDuringPageUnload value is true.

  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetBoolean(prefs::kAllowPopupsDuringPageUnload, false);
  AppendContentBrowserClientSwitches();
  EXPECT_FALSE(
      command_line().HasSwitch(switches::kAllowPopupsDuringPageUnload));
  // When the pref value is being set to true
  // the switch should be included.
  pref_service->SetBoolean(prefs::kAllowPopupsDuringPageUnload, true);
  AppendContentBrowserClientSwitches();
  EXPECT_TRUE(command_line().HasSwitch(switches::kAllowPopupsDuringPageUnload));
}

// Helper class to mark "https://ntp.com/" as an isolated origin.
class IsolatedOriginNTPBrowserTest : public InProcessBrowserTest,
                                     public InstantTestBase {
 public:
  IsolatedOriginNTPBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(https_test_server().InitializeAndListen());

    // Mark ntp.com (with an appropriate port from the test server) as an
    // isolated origin.
    GURL isolated_url(https_test_server().GetURL("ntp.com", "/"));
    command_line->AppendSwitchASCII(switches::kIsolateOrigins,
                                    isolated_url.spec());
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    https_test_server().StartAcceptingConnections();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IsolatedOriginNTPBrowserTest);
};

// Verifies that when the remote NTP URL has an origin which is also marked as
// an isolated origin (i.e., requiring a dedicated process), the NTP URL
// still loads successfully, and the resulting process is marked as an Instant
// process.  See https://crbug.com/755595.
IN_PROC_BROWSER_TEST_F(IsolatedOriginNTPBrowserTest,
                       IsolatedOriginDoesNotInterfereWithNTP) {
  GURL base_url =
      https_test_server().GetURL("ntp.com", "/instant_extended.html");
  GURL ntp_url =
      https_test_server().GetURL("ntp.com", "/instant_extended_ntp.html");
  InstantTestBase::Init(base_url, ntp_url, false);

  SetupInstant(browser());

  // Sanity check that a SiteInstance for a generic ntp.com URL requires a
  // dedicated process.
  content::BrowserContext* context = browser()->profile();
  GURL isolated_url(https_test_server().GetURL("ntp.com", "/title1.html"));
  scoped_refptr<SiteInstance> site_instance =
      SiteInstance::CreateForURL(context, isolated_url);
  EXPECT_TRUE(site_instance->RequiresDedicatedProcess());

  // The site URL for the NTP URL should resolve to a chrome-search:// URL via
  // GetEffectiveURL(), even if the NTP URL matches an isolated origin.
  GURL site_url(content::SiteInstance::GetSiteForURL(context, ntp_url));
  EXPECT_TRUE(site_url.SchemeIs(chrome::kChromeSearchScheme));

  // Navigate to the NTP URL and verify that the resulting process is marked as
  // an Instant process.
  ui_test_utils::NavigateToURL(browser(), ntp_url);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser()->profile());
  EXPECT_TRUE(instant_service->IsInstantProcess(
      contents->GetMainFrame()->GetProcess()->GetID()));

  // Navigating to a non-NTP URL on ntp.com should not result in an Instant
  // process.
  ui_test_utils::NavigateToURL(browser(), isolated_url);
  EXPECT_FALSE(instant_service->IsInstantProcess(
      contents->GetMainFrame()->GetProcess()->GetID()));
}

// Helper class to run tests on a simulated 512MB low-end device.
class SitePerProcessMemoryThresholdBrowserTest : public InProcessBrowserTest {
 public:
  SitePerProcessMemoryThresholdBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);

    // This way the test always sees the same amount of physical memory
    // (kLowMemoryDeviceThresholdMB = 512MB), regardless of how much memory is
    // available in the testing environment.
    command_line->AppendSwitch(switches::kEnableLowEndDeviceMode);
    EXPECT_EQ(512, base::SysInfo::AmountOfPhysicalMemoryMB());
  }

  // Some command-line switches override field trials - the tests need to be
  // skipped in this case.
  bool ShouldSkipBecauseOfConflictingCommandLineSwitches() {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kSitePerProcess))
      return true;

    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kDisableSiteIsolation))
      return true;

    return false;
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Initializing the expected embedder origins at runtime is required for
    // GetWebstoreLaunchURL(), which needs to have a proper ExtensionsClient
    // initialized.
#if !defined(OS_ANDROID)
    expected_embedder_origins_.push_back(
        url::Origin::Create(GaiaUrls::GetInstance()->gaia_url()));
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS)
    expected_embedder_origins_.push_back(
        url::Origin::Create(extension_urls::GetWebstoreLaunchURL()));
#endif
  }

 protected:
  // These are the origins we expect to be returned by
  // content::ChildProcessSecurityPolicy::GetIsolatedOrigins() even if
  // ContentBrowserClient::ShouldDisableSiteIsolation() returns true.
  std::vector<url::Origin> expected_embedder_origins_;

#if defined(OS_ANDROID)
  // On Android we don't expect any trial origins because the 512MB
  // physical memory used for testing is below the Android specific
  // hardcoded 1024MB memory limit that disables site isolation.
  const std::size_t kExpectedTrialOrigins = 0;
#else
  // All other platforms expect the single trial origin to be returned because
  // they don't have the memory limit that disables site isolation.
  const std::size_t kExpectedTrialOrigins = 1;
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(SitePerProcessMemoryThresholdBrowserTest);
};

IN_PROC_BROWSER_TEST_F(SitePerProcessMemoryThresholdBrowserTest,
                       SitePerProcessEnabled_HighThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  // 512MB of physical memory that the test simulates is below the 768MB
  // threshold.
  base::test::ScopedFeatureList memory_feature;
  memory_feature.InitAndEnableFeatureWithParameters(
      features::kSitePerProcessOnlyForHighMemoryClients,
      {{features::kSitePerProcessOnlyForHighMemoryClientsParamName, "768"}});

  base::test::ScopedFeatureList site_per_process;
  site_per_process.InitAndEnableFeature(features::kSitePerProcess);

  // Despite enabled site-per-process trial, there should be no isolation
  // because the device has too little memory.
  EXPECT_FALSE(
      content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessMemoryThresholdBrowserTest,
                       SitePerProcessEnabled_LowThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  // 512MB of physical memory that the test simulates is above the 128MB
  // threshold.
  base::test::ScopedFeatureList memory_feature;
  memory_feature.InitAndEnableFeatureWithParameters(
      features::kSitePerProcessOnlyForHighMemoryClients,
      {{features::kSitePerProcessOnlyForHighMemoryClientsParamName, "128"}});

  base::test::ScopedFeatureList site_per_process;
  site_per_process.InitAndEnableFeature(features::kSitePerProcess);

  // site-per-process trial is enabled, and the memory threshold is above the
  // memory present on the device.
  EXPECT_TRUE(content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessMemoryThresholdBrowserTest,
                       SitePerProcessEnabled_NoThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  base::test::ScopedFeatureList site_per_process;
  site_per_process.InitAndEnableFeature(features::kSitePerProcess);

#if defined(OS_ANDROID)
  // Expect false on Android because 512MB physical memory triggered by
  // kEnableLowEndDeviceMode in SetUpCommandLine() is below the 1024MB Android
  // specific memory limit which disbles site isolation for all sites.
  EXPECT_FALSE(
      content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
#else
  EXPECT_TRUE(content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
#endif
}

IN_PROC_BROWSER_TEST_F(SitePerProcessMemoryThresholdBrowserTest,
                       SitePerProcessDisabled_HighThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  // 512MB of physical memory that the test simulates is below the 768MB
  // threshold.
  base::test::ScopedFeatureList memory_feature;
  memory_feature.InitAndEnableFeatureWithParameters(
      features::kSitePerProcessOnlyForHighMemoryClients,
      {{features::kSitePerProcessOnlyForHighMemoryClientsParamName, "768"}});

  base::test::ScopedFeatureList site_per_process;
  site_per_process.InitAndDisableFeature(features::kSitePerProcess);

  // site-per-process trial is disabled, so isolation should be disabled
  // (i.e. the memory threshold should be ignored).
  EXPECT_FALSE(
      content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessMemoryThresholdBrowserTest,
                       SitePerProcessDisabled_LowThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  // 512MB of physical memory that the test simulates is above the 128MB
  // threshold.
  base::test::ScopedFeatureList memory_feature;
  memory_feature.InitAndEnableFeatureWithParameters(
      features::kSitePerProcessOnlyForHighMemoryClients,
      {{features::kSitePerProcessOnlyForHighMemoryClientsParamName, "128"}});

  base::test::ScopedFeatureList site_per_process;
  site_per_process.InitAndDisableFeature(features::kSitePerProcess);

  // site-per-process trial is disabled, so isolation should be disabled
  // (i.e. the memory threshold should be ignored).
  EXPECT_FALSE(
      content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessMemoryThresholdBrowserTest,
                       SitePerProcessDisabled_NoThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  base::test::ScopedFeatureList site_per_process;
  site_per_process.InitAndDisableFeature(features::kSitePerProcess);

  // site-per-process trial is disabled, so isolation should be disabled
  // (i.e. the memory threshold should be ignored).
  EXPECT_FALSE(
      content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessMemoryThresholdBrowserTest,
                       TrialIsolatedOrigins_HighThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  // 512MB of physical memory that the test simulates is below the 768MB
  // threshold.
  base::test::ScopedFeatureList memory_feature;
  memory_feature.InitAndEnableFeatureWithParameters(
      features::kSitePerProcessOnlyForHighMemoryClients,
      {{features::kSitePerProcessOnlyForHighMemoryClientsParamName, "768"}});

  const url::Origin trial_origin = url::Origin::Create(GURL("http://foo.com/"));
  base::test::ScopedFeatureList isolated_origins_feature;
  isolated_origins_feature.InitAndEnableFeatureWithParameters(
      features::kIsolateOrigins, {{features::kIsolateOriginsFieldTrialParamName,
                                   trial_origin.Serialize()}});
  SiteIsolationPolicy::ApplyGlobalIsolatedOrigins();

  auto* cpsp = content::ChildProcessSecurityPolicy::GetInstance();
  std::vector<url::Origin> isolated_origins = cpsp->GetIsolatedOrigins();
  EXPECT_EQ(expected_embedder_origins_.size(), isolated_origins.size());

  // Verify that the expected embedder origins are present even though site
  // isolation has been disabled and the trial origins should not be present.
  EXPECT_THAT(expected_embedder_origins_,
              ::testing::IsSubsetOf(isolated_origins));

  // Verify that the trial origin is not present.
  EXPECT_THAT(isolated_origins,
              ::testing::Not(::testing::Contains(trial_origin)));
}

IN_PROC_BROWSER_TEST_F(SitePerProcessMemoryThresholdBrowserTest,
                       TrialIsolatedOrigins_LowThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  // 512MB of physical memory that the test simulates is above the 128MB
  // threshold.
  base::test::ScopedFeatureList memory_feature;
  memory_feature.InitAndEnableFeatureWithParameters(
      features::kSitePerProcessOnlyForHighMemoryClients,
      {{features::kSitePerProcessOnlyForHighMemoryClientsParamName, "128"}});

  const url::Origin trial_origin = url::Origin::Create(GURL("http://foo.com/"));
  base::test::ScopedFeatureList isolated_origins_feature;
  isolated_origins_feature.InitAndEnableFeatureWithParameters(
      features::kIsolateOrigins, {{features::kIsolateOriginsFieldTrialParamName,
                                   trial_origin.Serialize()}});
  SiteIsolationPolicy::ApplyGlobalIsolatedOrigins();

  auto* cpsp = content::ChildProcessSecurityPolicy::GetInstance();
  std::vector<url::Origin> isolated_origins = cpsp->GetIsolatedOrigins();
  EXPECT_EQ(1u + expected_embedder_origins_.size(), isolated_origins.size());
  EXPECT_THAT(expected_embedder_origins_,
              ::testing::IsSubsetOf(isolated_origins));

  // Verify that the trial origin is present.
  EXPECT_THAT(isolated_origins, ::testing::Contains(trial_origin));
}

IN_PROC_BROWSER_TEST_F(SitePerProcessMemoryThresholdBrowserTest,
                       TrialIsolatedOrigins_NoThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  const url::Origin trial_origin = url::Origin::Create(GURL("http://foo.com/"));
  base::test::ScopedFeatureList isolated_origins_feature;
  isolated_origins_feature.InitAndEnableFeatureWithParameters(
      features::kIsolateOrigins, {{features::kIsolateOriginsFieldTrialParamName,
                                   trial_origin.Serialize()}});
  SiteIsolationPolicy::ApplyGlobalIsolatedOrigins();

  auto* cpsp = content::ChildProcessSecurityPolicy::GetInstance();
  std::vector<url::Origin> isolated_origins = cpsp->GetIsolatedOrigins();
  EXPECT_EQ(kExpectedTrialOrigins + expected_embedder_origins_.size(),
            isolated_origins.size());
  EXPECT_THAT(expected_embedder_origins_,
              ::testing::IsSubsetOf(isolated_origins));

  if (kExpectedTrialOrigins > 0) {
    // Verify that the trial origin is present.
    EXPECT_THAT(isolated_origins, ::testing::Contains(trial_origin));
  } else {
    EXPECT_THAT(isolated_origins,
                ::testing::Not(::testing::Contains(trial_origin)));
  }
}

// Helper class to test window creation from NTP.
class OpenWindowFromNTPBrowserTest : public InProcessBrowserTest,
                                     public InstantTestBase {
 public:
  OpenWindowFromNTPBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(https_test_server().InitializeAndListen());
    https_test_server().StartAcceptingConnections();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OpenWindowFromNTPBrowserTest);
};

// Test checks that navigations from NTP tab to URLs with same host as NTP but
// different path do not reuse NTP SiteInstance. See https://crbug.com/859062
// for details.
IN_PROC_BROWSER_TEST_F(OpenWindowFromNTPBrowserTest,
                       TransferFromNTPCreateNewTab) {
  GURL search_url =
      https_test_server().GetURL("ntp.com", "/instant_extended.html");
  GURL ntp_url =
      https_test_server().GetURL("ntp.com", "/instant_extended_ntp.html");
  InstantTestBase::Init(search_url, ntp_url, false);

  SetupInstant(browser());

  // Navigate to the NTP URL and verify that the resulting process is marked as
  // an Instant process.
  ui_test_utils::NavigateToURL(browser(), ntp_url);
  content::WebContents* ntp_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser()->profile());
  EXPECT_TRUE(instant_service->IsInstantProcess(
      ntp_tab->GetMainFrame()->GetProcess()->GetID()));

  // Execute script that creates new window from ntp tab with
  // ntp.com/title1.html as target url. Host is same as remote-ntp host, yet
  // path is different.
  GURL generic_url(https_test_server().GetURL("ntp.com", "/title1.html"));
  content::TestNavigationObserver opened_tab_observer(nullptr);
  opened_tab_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(
      ExecuteScript(ntp_tab, "window.open('" + generic_url.spec() + "');"));
  opened_tab_observer.Wait();
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  content::WebContents* opened_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Wait until newly opened tab is fully loaded.
  EXPECT_TRUE(WaitForLoadStop(opened_tab));

  EXPECT_NE(opened_tab, ntp_tab);
  EXPECT_EQ(generic_url, opened_tab->GetLastCommittedURL());
  // New created tab should not reside in an Instant process.
  EXPECT_FALSE(instant_service->IsInstantProcess(
      opened_tab->GetMainFrame()->GetProcess()->GetID()));
}

class PrefersColorSchemeTest : public testing::WithParamInterface<bool>,
                               public InProcessBrowserTest {
 protected:
  PrefersColorSchemeTest() : theme_client_(&test_theme_) {}

  ~PrefersColorSchemeTest() {
    CHECK_EQ(&theme_client_, SetBrowserClientForTesting(original_client_));
  }

  const char* ExpectedColorScheme() const {
    return GetParam() ? "dark" : "light";
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "MediaQueryPrefersColorScheme");
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    original_client_ = SetBrowserClientForTesting(&theme_client_);
  }

 protected:
  ui::TestNativeTheme test_theme_;

 private:
  content::ContentBrowserClient* original_client_ = nullptr;

  class ChromeContentBrowserClientWithWebTheme
      : public ChromeContentBrowserClient {
   public:
    explicit ChromeContentBrowserClientWithWebTheme(
        const ui::NativeTheme* theme)
        : theme_(theme) {}

   protected:
    const ui::NativeTheme* GetWebTheme() const override { return theme_; }

   private:
    const ui::NativeTheme* const theme_;
  };

  ChromeContentBrowserClientWithWebTheme theme_client_;
};

IN_PROC_BROWSER_TEST_P(PrefersColorSchemeTest, PrefersColorScheme) {
  test_theme_.SetDarkMode(GetParam());
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->GetRenderViewHost()
      ->OnWebkitPreferencesChanged();
  ui_test_utils::NavigateToURL(
      browser(),
      ui_test_utils::GetTestUrl(
          base::FilePath(base::FilePath::kCurrentDirectory),
          base::FilePath(FILE_PATH_LITERAL("prefers-color-scheme.html"))));
  base::string16 tab_title;
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &tab_title));
  EXPECT_EQ(base::ASCIIToUTF16(ExpectedColorScheme()), tab_title);
}

IN_PROC_BROWSER_TEST_P(PrefersColorSchemeTest, FeatureOverridesChromeSchemes) {
  test_theme_.SetDarkMode(true);

  base::test::ScopedFeatureList features;
  features.InitWithFeatureState(features::kWebUIDarkMode, GetParam());

  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIDownloadsURL));

  bool matches;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      base::StringPrintf("window.domAutomationController.send(window."
                         "matchMedia('(prefers-color-scheme: %s)').matches)",
                         ExpectedColorScheme()),
      &matches));
  EXPECT_TRUE(matches);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
IN_PROC_BROWSER_TEST_P(PrefersColorSchemeTest, FeatureOverridesPdfUI) {
  test_theme_.SetDarkMode(true);

  base::test::ScopedFeatureList features;
  features.InitWithFeatureState(features::kWebUIDarkMode, GetParam());

  std::string pdf_extension_url(extensions::kExtensionScheme);
  pdf_extension_url.append(url::kStandardSchemeSeparator);
  pdf_extension_url.append(extension_misc::kPdfExtensionId);
  GURL pdf_index = GURL(pdf_extension_url).Resolve("/index.html");
  ui_test_utils::NavigateToURL(browser(), pdf_index);

  bool matches;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      base::StringPrintf("window.domAutomationController.send(window."
                         "matchMedia('(prefers-color-scheme: %s)').matches)",
                         ExpectedColorScheme()),
      &matches));
  EXPECT_TRUE(matches);
}
#endif

INSTANTIATE_TEST_SUITE_P(All, PrefersColorSchemeTest, testing::Bool());

#if !defined(OS_MACOSX)
class ForcedColorsTest : public testing::WithParamInterface<bool>,
                         public InProcessBrowserTest {
 protected:
  ForcedColorsTest() : theme_client_(&test_theme_) {}

  ~ForcedColorsTest() {
    CHECK_EQ(&theme_client_, SetBrowserClientForTesting(original_client_));
  }

  const char* ExpectedForcedColors() const {
    return GetParam() ? "active" : "none";
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "ForcedColors");
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    original_client_ = SetBrowserClientForTesting(&theme_client_);
  }

 protected:
  ui::TestNativeTheme test_theme_;

 private:
  content::ContentBrowserClient* original_client_ = nullptr;

  class ChromeContentBrowserClientWithWebTheme
      : public ChromeContentBrowserClient {
   public:
    explicit ChromeContentBrowserClientWithWebTheme(
        const ui::NativeTheme* theme)
        : theme_(theme) {}

   protected:
    const ui::NativeTheme* GetWebTheme() const override { return theme_; }

   private:
    const ui::NativeTheme* const theme_;
  };

  ChromeContentBrowserClientWithWebTheme theme_client_;
};

IN_PROC_BROWSER_TEST_P(ForcedColorsTest, ForcedColors) {
  test_theme_.SetUsesHighContrastColors(GetParam());
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->GetRenderViewHost()
      ->OnWebkitPreferencesChanged();
  ui_test_utils::NavigateToURL(
      browser(), ui_test_utils::GetTestUrl(
                     base::FilePath(base::FilePath::kCurrentDirectory),
                     base::FilePath(FILE_PATH_LITERAL("forced-colors.html"))));
  base::string16 tab_title;
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &tab_title));
  EXPECT_EQ(base::ASCIIToUTF16(ExpectedForcedColors()), tab_title);
}

INSTANTIATE_TEST_SUITE_P(All, ForcedColorsTest, testing::Bool());
#endif  // !defined(OS_MACOSX)

class ProtocolHandlerTest : public InProcessBrowserTest {
 public:
  ProtocolHandlerTest() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  void AddProtocolHandler(const std::string& scheme,
                          const std::string& redirect_template) {
    protocol_handler_registry()->OnAcceptRegisterProtocolHandler(
        ProtocolHandler::CreateProtocolHandler(scheme,
                                               GURL(redirect_template)));
  }

  ProtocolHandlerRegistry* protocol_handler_registry() {
    return ProtocolHandlerRegistryFactory::GetInstance()->GetForBrowserContext(
        browser()->profile());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProtocolHandlerTest);
};

IN_PROC_BROWSER_TEST_F(ProtocolHandlerTest, CustomHandler) {
  AddProtocolHandler("abc", "https://abc.xyz/?url=%s");

  ui_test_utils::NavigateToURL(browser(), GURL("abc:something"));

  base::string16 expected_title = base::ASCIIToUTF16("abc.xyz");
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// This is a regression test for crbug.com/969177.
IN_PROC_BROWSER_TEST_F(ProtocolHandlerTest, HandlersIgnoredWhenDisabled) {
  AddProtocolHandler("abc", "https://abc.xyz/?url=%s");
  protocol_handler_registry()->Disable();

  ui_test_utils::NavigateToURL(browser(), GURL("abc:something"));

  base::string16 tab_title;
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &tab_title));
  EXPECT_EQ(base::ASCIIToUTF16("about:blank"), tab_title);
}

#if defined(OS_CHROMEOS)
// Tests that if a protocol handler is registered for a scheme, an external
// program (another Chrome tab in this case) is not launched to handle the
// navigation. This is a regression test for crbug.com/963133.
IN_PROC_BROWSER_TEST_F(ProtocolHandlerTest, ExternalProgramNotLaunched) {
  ui_test_utils::NavigateToURL(browser(), GURL("mailto:bob@example.com"));

  // If an external program (Chrome) was launched, it will result in a second
  // tab being opened.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Make sure the protocol handler redirected the navigation.
  base::string16 expected_title = base::ASCIIToUTF16("mail.google.com");
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}
#endif

}  // namespace content
