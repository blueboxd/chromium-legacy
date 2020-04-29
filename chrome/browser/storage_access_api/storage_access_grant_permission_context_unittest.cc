// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_access_api/storage_access_grant_permission_context.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace {

constexpr char kInsecureURL[] = "http://www.example.com";
constexpr char kSecureURL[] = "https://www.example.com";
constexpr char kAlternateURL[] = "https://embedder_example.test";

void SaveResult(ContentSetting* content_setting_result,
                ContentSetting content_setting) {
  DCHECK(content_setting_result);
  *content_setting_result = content_setting;
}

}  // namespace

class StorageAccessGrantPermissionContextTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Ensure we are navigated to some page so that the proper views get setup.
    NavigateAndCommit(GURL(kInsecureURL));

    // Create PermissionRequestManager.
    permissions::PermissionRequestManager::CreateForWebContents(web_contents());

    mock_permission_prompt_factory_ =
        std::make_unique<permissions::MockPermissionPromptFactory>(
            permissions::PermissionRequestManager::FromWebContents(
                web_contents()));
  }

  void TearDown() override {
    mock_permission_prompt_factory_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // Helper to request storage access on enough unique embedding_origin GURLs
  // from |requesting_origin| to ensure that all potential implicit grants will
  // be granted.
  void ExhaustImplicitGrants(
      const GURL& requesting_origin,
      StorageAccessGrantPermissionContext& permission_context) {
    permissions::PermissionRequestID fake_id(
        /*render_process_id=*/0, /*render_frame_id=*/0, /*request_id=*/0);

    permissions::PermissionRequestManager* manager =
        permissions::PermissionRequestManager::FromWebContents(web_contents());
    DCHECK(manager);
    for (int grant_id = 0; grant_id < kDefaultImplicitGrantLimit; grant_id++) {
      const GURL embedding_origin(std::string(url::kHttpsScheme) +
                                  "://example_embedder_" +
                                  base::NumberToString(grant_id) + ".com");

      ContentSetting result = CONTENT_SETTING_DEFAULT;
      permission_context.DecidePermission(
          web_contents(), fake_id, requesting_origin, embedding_origin,
          /*user_gesture=*/true, base::BindOnce(&SaveResult, &result));
      base::RunLoop().RunUntilIdle();

      // We should not have a prompt showing up right now.
      EXPECT_FALSE(manager->IsRequestInProgress());
    }
  }

 private:
  std::unique_ptr<permissions::MockPermissionPromptFactory>
      mock_permission_prompt_factory_;
};

TEST_F(StorageAccessGrantPermissionContextTest, InsecureOriginsAreAllowed) {
  StorageAccessGrantPermissionContext permission_context(profile());
  EXPECT_TRUE(permission_context.IsPermissionAvailableToOrigins(
      GURL(kInsecureURL), GURL(kInsecureURL)));
  EXPECT_TRUE(permission_context.IsPermissionAvailableToOrigins(
      GURL(kInsecureURL), GURL(kSecureURL)));
}

// When the Storage Access API feature is disabled we should block the
// permission request.
TEST_F(StorageAccessGrantPermissionContextTest,
       PermissionBlockedWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_disable;
  scoped_disable.InitAndDisableFeature(blink::features::kStorageAccessAPI);

  StorageAccessGrantPermissionContext permission_context(profile());
  permissions::PermissionRequestID fake_id(
      /*render_process_id=*/0, /*render_frame_id=*/0, /*request_id=*/0);

  ContentSetting result = CONTENT_SETTING_DEFAULT;
  permission_context.DecidePermission(
      web_contents(), fake_id, GURL(kSecureURL), GURL(kSecureURL),
      /*user_gesture=*/true, base::BindOnce(&SaveResult, &result));
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result);
}

// When the Storage Access API feature is enabled and we have a user gesture we
// should get a decision.
TEST_F(StorageAccessGrantPermissionContextTest,
       PermissionDecidedWhenFeatureEnabled) {
  base::test::ScopedFeatureList scoped_enable;
  scoped_enable.InitAndEnableFeature(blink::features::kStorageAccessAPI);

  StorageAccessGrantPermissionContext permission_context(profile());
  permissions::PermissionRequestID fake_id(
      /*render_process_id=*/0, /*render_frame_id=*/0, /*request_id=*/0);

  const GURL requesting_origin(kAlternateURL);
  const GURL embedding_origin(kSecureURL);

  // Ensure all our implicit grants are taken care of before we proceed to
  // validate.
  ExhaustImplicitGrants(requesting_origin, permission_context);

  ContentSetting result = CONTENT_SETTING_DEFAULT;
  permission_context.DecidePermission(
      web_contents(), fake_id, requesting_origin, embedding_origin,
      /*user_gesture=*/true, base::BindOnce(&SaveResult, &result));
  base::RunLoop().RunUntilIdle();

  // We should get a prompt showing up right now.
  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());
  DCHECK(manager);
  EXPECT_TRUE(manager->IsRequestInProgress());

  // Verify the prompt is showing text that includes both of the origins we
  // expect.
  permissions::PermissionRequest* request = manager->Requests().front();
  ASSERT_TRUE(request);
  ASSERT_EQ(1u, manager->Requests().size());
  EXPECT_NE(request->GetMessageTextFragment().find(
                base::UTF8ToUTF16(requesting_origin.host())),
            base::string16::npos);
  EXPECT_NE(request->GetMessageTextFragment().find(
                base::UTF8ToUTF16(embedding_origin.host())),
            base::string16::npos);

  // Close the prompt and validate we get the expected setting back in our
  // callback.
  manager->Closing();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CONTENT_SETTING_ASK, result);
}

// No user gesture should force a permission rejection.
TEST_F(StorageAccessGrantPermissionContextTest,
       PermissionDeniedWithoutUserGesture) {
  base::test::ScopedFeatureList scoped_enable;
  scoped_enable.InitAndEnableFeature(blink::features::kStorageAccessAPI);

  StorageAccessGrantPermissionContext permission_context(profile());
  permissions::PermissionRequestID fake_id(
      /*render_process_id=*/0, /*render_frame_id=*/0, /*request_id=*/0);

  ContentSetting result = CONTENT_SETTING_DEFAULT;
  permission_context.DecidePermission(
      web_contents(), fake_id, GURL(kSecureURL), GURL(kSecureURL),
      /*user_gesture=*/false, base::BindOnce(&SaveResult, &result));
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result);
}

TEST_F(StorageAccessGrantPermissionContextTest,
       PermissionStatusBlockedWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_disable;
  scoped_disable.InitAndDisableFeature(blink::features::kStorageAccessAPI);

  StorageAccessGrantPermissionContext permission_context(profile());

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            permission_context
                .GetPermissionStatus(/*render_frame_host=*/nullptr,
                                     GURL(kSecureURL), GURL(kSecureURL))
                .content_setting);
}

TEST_F(StorageAccessGrantPermissionContextTest,
       PermissionStatusAsksWhenFeatureEnabled) {
  base::test::ScopedFeatureList scoped_enable;
  scoped_enable.InitAndEnableFeature(blink::features::kStorageAccessAPI);

  StorageAccessGrantPermissionContext permission_context(profile());

  EXPECT_EQ(CONTENT_SETTING_ASK,
            permission_context
                .GetPermissionStatus(/*render_frame_host=*/nullptr,
                                     GURL(kSecureURL), GURL(kSecureURL))
                .content_setting);
}

// Validate that each requesting origin has its own implicit grant limit. If
// the limit for one origin is exhausted it should not affect another.
TEST_F(StorageAccessGrantPermissionContextTest,
       ImplicitGrantLimitPerRequestingOrigin) {
  base::test::ScopedFeatureList scoped_enable;
  scoped_enable.InitAndEnableFeature(blink::features::kStorageAccessAPI);

  StorageAccessGrantPermissionContext permission_context(profile());
  permissions::PermissionRequestID fake_id(
      /*render_process_id=*/0, /*render_frame_id=*/0, /*request_id=*/0);

  const GURL requesting_origin_1(kAlternateURL);
  const GURL requesting_origin_2(kInsecureURL);
  const GURL embedding_origin(kSecureURL);

  // Ensure all our implicit grants are taken care of for |requesting_origin_1|
  // before we proceed to validate.
  ExhaustImplicitGrants(requesting_origin_1, permission_context);

  ContentSetting result = CONTENT_SETTING_DEFAULT;
  permission_context.DecidePermission(
      web_contents(), fake_id, requesting_origin_1, embedding_origin,
      /*user_gesture=*/true, base::BindOnce(&SaveResult, &result));
  base::RunLoop().RunUntilIdle();

  // We should get a prompt showing up right now.
  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());
  DCHECK(manager);
  EXPECT_TRUE(manager->IsRequestInProgress());

  // Close the prompt and validate we get the expected setting back in our
  // callback.
  manager->Closing();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CONTENT_SETTING_ASK, result);

  // However now if a different requesting origin makes a request we should see
  // it gets auto-granted as the limit has not been reached for it yet.
  result = CONTENT_SETTING_DEFAULT;
  permission_context.DecidePermission(
      web_contents(), fake_id, requesting_origin_2, embedding_origin,
      /*user_gesture=*/true, base::BindOnce(&SaveResult, &result));
  base::RunLoop().RunUntilIdle();

  // We should have no prompts still and our latest result should be an allow.
  EXPECT_FALSE(manager->IsRequestInProgress());
  EXPECT_EQ(CONTENT_SETTING_ALLOW, result);
}
