// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/action_tracker.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "chrome/browser/extensions/api/declarative_net_request/dnr_test_base.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "content/public/common/resource_type.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {
namespace declarative_net_request {

namespace dnr_api = api::declarative_net_request;

namespace {

constexpr char kJSONRulesFilename[] = "rules_file.json";
const base::FilePath::CharType kJSONRulesetFilepath[] =
    FILE_PATH_LITERAL("rules_file.json");

constexpr int64_t kNavigationId = 1;

class ActionTrackerTest : public DNRTestBase {
 public:
  ActionTrackerTest() = default;
  ActionTrackerTest(const ActionTrackerTest& other) = delete;
  ActionTrackerTest& operator=(const ActionTrackerTest& other) = delete;

  void SetUp() override {
    DNRTestBase::SetUp();
    action_tracker_ = std::make_unique<ActionTracker>(browser_context());
  }

 protected:
  using RequestActionType = RequestAction::Type;

  // Helper to load an extension. |has_feedback_permission| specifies whether
  // the extension will have the declarativeNetRequestFeedback permission.
  void LoadExtension(const std::string& extension_dirname,
                     bool has_feedback_permission) {
    base::FilePath extension_dir =
        temp_dir().GetPath().AppendASCII(extension_dirname);

    // Create extension directory.
    ASSERT_TRUE(base::CreateDirectory(extension_dir));
    WriteManifestAndRuleset(
        extension_dir, kJSONRulesetFilepath, kJSONRulesFilename,
        std::vector<TestRule>(),
        std::vector<std::string>({URLPattern::kAllUrlsPattern}),
        false /* has_background_script */, has_feedback_permission);

    last_loaded_extension_ =
        CreateExtensionLoader()->LoadExtension(extension_dir);
    ASSERT_TRUE(last_loaded_extension_);

    ExtensionRegistry::Get(browser_context())
        ->AddEnabled(last_loaded_extension_);
  }

  // Helper to create a RequestAction for the given |extension_id|.
  RequestAction CreateRequestAction(const ExtensionId& extension_id) {
    return RequestAction(RequestActionType::BLOCK, kMinValidID,
                         kDefaultPriority, dnr_api::SOURCE_TYPE_MANIFEST,
                         extension_id);
  }

  // Returns renderer-initiated request params for the given |url|.
  WebRequestInfoInitParams GetRequestParamsForURL(base::StringPiece url,
                                                  content::ResourceType type,
                                                  int tab_id) {
    const int kRendererId = 1;
    WebRequestInfoInitParams info;
    info.url = GURL(url);
    info.type = type;
    info.render_process_id = kRendererId;
    info.frame_data.tab_id = tab_id;

    if (type == content::ResourceType::kMainFrame) {
      info.navigation_id = kNavigationId;
      info.is_navigation_request = true;
    }

    return info;
  }

  const Extension* last_loaded_extension() const {
    return last_loaded_extension_.get();
  }

  ActionTracker* action_tracker() { return action_tracker_.get(); }

 private:
  scoped_refptr<const Extension> last_loaded_extension_;
  std::unique_ptr<ActionTracker> action_tracker_;
};

// Test that rules matched will only be recorded for extensions with the
// declarativeNetRequestFeedback permission.
TEST_P(ActionTrackerTest, GetMatchedRulesNoPermission) {
  // Load an extension with the declarativeNetRequestFeedback permission.
  ASSERT_NO_FATAL_FAILURE(
      LoadExtension("test_extension", true /* has_feedback_permission */));
  const Extension* extension_1 = last_loaded_extension();

  const int tab_id = 1;

  // Record a rule match for a main-frame navigation request.
  WebRequestInfo request_1(GetRequestParamsForURL(
      "http://one.com", content::ResourceType::kMainFrame, tab_id));
  action_tracker()->OnRuleMatched(CreateRequestAction(extension_1->id()),
                                  request_1);

  // Record a rule match for a non-navigation request.
  WebRequestInfo request_2(GetRequestParamsForURL(
      "http://one.com", content::ResourceType::kSubResource, tab_id));
  action_tracker()->OnRuleMatched(CreateRequestAction(extension_1->id()),
                                  request_2);

  // For |extension_1|, one rule match should be recorded for |rules_tracked_|
  // and one for |pending_navigation_actions_|.
  EXPECT_EQ(1, action_tracker()->GetMatchedRuleCountForTest(extension_1->id(),
                                                            tab_id));
  EXPECT_EQ(1, action_tracker()->GetPendingRuleCountForTest(extension_1->id(),
                                                            kNavigationId));

  // Load an extension without the declarativeNetRequestFeedback permission.
  ASSERT_NO_FATAL_FAILURE(
      LoadExtension("test_extension_2", false /* has_feedback_permission */));
  const Extension* extension_2 = last_loaded_extension();

  // The same requests are matched for |extension_2|.
  action_tracker()->OnRuleMatched(CreateRequestAction(extension_2->id()),
                                  request_1);
  action_tracker()->OnRuleMatched(CreateRequestAction(extension_2->id()),
                                  request_2);

  // Since |extension_2| does not have the feedback permission, no rule matches
  // should be recorded.
  EXPECT_EQ(0, action_tracker()->GetMatchedRuleCountForTest(extension_2->id(),
                                                            tab_id));
  EXPECT_EQ(0, action_tracker()->GetPendingRuleCountForTest(extension_2->id(),
                                                            kNavigationId));

  // Clean up the internal state of |action_tracker_|.
  action_tracker()->ClearPendingNavigation(kNavigationId);
  action_tracker()->ClearTabData(tab_id);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ActionTrackerTest,
                         ::testing::Values(ExtensionLoadType::PACKED,
                                           ExtensionLoadType::UNPACKED));

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions
