// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "content/browser/background_sync/background_sync_base_browsertest.h"
#include "content/public/common/content_features.h"

namespace content {

class PeriodicBackgroundSyncBrowserTest : public BackgroundSyncBaseBrowserTest {
 public:
  PeriodicBackgroundSyncBrowserTest() {}
  ~PeriodicBackgroundSyncBrowserTest() override {}

  void SetUpOnMainThread() override;
  bool Register(const std::string& tag, int min_interval_ms);
  bool RegisterNoMinInterval(const std::string& tag);
  bool RegisterFromServiceWorker(const std::string& tag, int min_interval_ms);
  bool RegisterFromServiceWorkerNoMinInterval(const std::string& tag);
  bool HasTag(const std::string& tag);
  bool HasTagFromServiceWorker(const std::string& tag);
  bool Unregister(const std::string& tag);
  bool UnregisterFromServiceWorker(const std::string& tag);

 protected:
  base::SimpleTestClock clock_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(PeriodicBackgroundSyncBrowserTest);
};

void PeriodicBackgroundSyncBrowserTest::SetUpOnMainThread() {
  scoped_feature_list_.InitAndEnableFeature(features::kPeriodicBackgroundSync);
  BackgroundSyncBaseBrowserTest::SetUpOnMainThread();
}

bool PeriodicBackgroundSyncBrowserTest::Register(const std::string& tag,
                                                 int min_interval_ms) {
  std::string script_result;
  EXPECT_TRUE(
      RunScript(base::StringPrintf("%s('%s', %d);", "registerPeriodicSync",
                                   tag.c_str(), min_interval_ms),
                &script_result));
  return script_result == BuildExpectedResult(tag, "registered");
}

bool PeriodicBackgroundSyncBrowserTest::RegisterNoMinInterval(
    const std::string& tag) {
  std::string script_result;
  EXPECT_TRUE(RunScript(
      base::StringPrintf("%s('%s');", "registerPeriodicSync", tag.c_str()),
      &script_result));
  return script_result == BuildExpectedResult(tag, "registered");
}

bool PeriodicBackgroundSyncBrowserTest::RegisterFromServiceWorker(
    const std::string& tag,
    int min_interval_ms) {
  std::string script_result;
  EXPECT_TRUE(
      RunScript(base::StringPrintf("%s('%s', %d);",
                                   "registerPeriodicSyncFromServiceWorker",
                                   tag.c_str(), min_interval_ms),
                &script_result));
  return script_result == BuildExpectedResult(tag, "register sent to SW");
}

bool PeriodicBackgroundSyncBrowserTest::RegisterFromServiceWorkerNoMinInterval(
    const std::string& tag) {
  std::string script_result;
  EXPECT_TRUE(
      RunScript(BuildScriptString("registerPeriodicSyncFromServiceWorker", tag),
                &script_result));
  return script_result == BuildExpectedResult(tag, "register sent to SW");
}

bool PeriodicBackgroundSyncBrowserTest::HasTag(const std::string& tag) {
  std::string script_result;
  EXPECT_TRUE(
      RunScript(BuildScriptString("hasPeriodicSyncTag", tag), &script_result));
  return script_result == BuildExpectedResult(tag, "found");
}

bool PeriodicBackgroundSyncBrowserTest::HasTagFromServiceWorker(
    const std::string& tag) {
  std::string script_result;
  EXPECT_TRUE(
      RunScript(BuildScriptString("hasPeriodicSyncTagFromServiceWorker", tag),
                &script_result));
  return (script_result == "ok - hasTag sent to SW");
}

bool PeriodicBackgroundSyncBrowserTest::Unregister(const std::string& tag) {
  std::string script_result;
  EXPECT_TRUE(RunScript(BuildScriptString("unregister", tag), &script_result));
  return script_result == BuildExpectedResult(tag, "unregistered");
}

bool PeriodicBackgroundSyncBrowserTest::UnregisterFromServiceWorker(
    const std::string& tag) {
  std::string script_result;
  EXPECT_TRUE(RunScript(BuildScriptString("unregisterFromServiceWorker", tag),
                        &script_result));
  return script_result == BuildExpectedResult(tag, "unregister sent to SW");
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest,
                       RegisterFromControlledDocument) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  EXPECT_TRUE(Register("foo", /* min_interval_ms= */ 1000));
  EXPECT_TRUE(Unregister("foo"));
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest,
                       RegisterNoMinInterval) {
  EXPECT_TRUE(RegisterServiceWorker());

  EXPECT_TRUE(RegisterNoMinInterval("foo"));
  EXPECT_TRUE(Unregister("foo"));
}

// Verify that Register works in a service worker
IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest,
                       RegisterFromServiceWorker) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));

  EXPECT_TRUE(RegisterFromServiceWorker("foo_sw", /* min_interval_ms= */ 10));
  EXPECT_TRUE(PopConsole("ok - foo_sw registered in SW"));
  EXPECT_TRUE(Unregister("foo"));
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest,
                       RegisterFromServiceWorkerNoMinInterval) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));

  EXPECT_TRUE(RegisterFromServiceWorkerNoMinInterval("foo_sw"));
  EXPECT_TRUE(PopConsole("ok - foo_sw registered in SW"));
  EXPECT_TRUE(Unregister("foo"));
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest, FindATag) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  EXPECT_TRUE(Register("foo", /* min_interval_ms= */ 1000));
  EXPECT_TRUE(HasTag("foo"));
  EXPECT_TRUE(Unregister("foo"));
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest,
                       FindATagFromServiceWorker) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  EXPECT_TRUE(Register("foo", /* min_interval_ms= */ 1000));
  EXPECT_TRUE(HasTagFromServiceWorker("foo"));
  EXPECT_TRUE(PopConsole("ok - foo found in SW"));
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest,
                       UnregisterFromServiceWorker) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));

  EXPECT_TRUE(RegisterNoMinInterval("foo"));
  EXPECT_TRUE(HasTag("foo"));
  EXPECT_TRUE(UnregisterFromServiceWorker("foo"));
  EXPECT_TRUE(PopConsole("ok - foo unregistered in SW"));
}

}  // namespace content
