// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"

namespace web_components_prefs {

class WebComponentsV0Test : public policy::PolicyTest {
  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    policy::PolicyMap policies;
    policies.Set(policy::key::kWebComponentsV0Enabled,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD,
                 std::make_unique<base::Value>(true), nullptr);
    provider_.UpdateChromePolicy(policies);
  }
};

IN_PROC_BROWSER_TEST_F(WebComponentsV0Test, CheckWebComponentsV0Enabled) {
  ASSERT_TRUE(embedded_test_server()->Start());

  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kWebComponentsV0Enabled));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kWebComponentsV0Enabled));

  GURL url(embedded_test_server()->GetURL("/empty.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  constexpr char kScript[] =
      R"({
          let link = document.createElement('link');
          link.setAttribute('rel','import');
          let div = document.createElement('div');
          try { div.createShadowRoot(); } catch {}
          try { document.registerElement('test-element'); } catch {}
          const observer = new ReportingObserver((reports, observer) => {
            var needed = ['HTMLImports','ElementCreateShadowRoot',
               'DocumentRegisterElement'];
            for (const report of reports) {
              needed = needed.filter(item => item !== report.body.id);
            }
            var allEnabled = needed.length == 0;
            window.domAutomationController.send(allEnabled);
          }, {types: ['deprecation'], buffered: true});
          observer.observe();
         })";
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::DOMMessageQueue message_queue;
  content::ExecuteScriptAsync(web_contents, kScript);
  std::string message;
  EXPECT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("true", message);
}

}  // namespace web_components_prefs
