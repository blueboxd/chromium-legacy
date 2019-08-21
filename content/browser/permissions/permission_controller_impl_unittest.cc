// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_controller_impl.h"

#include <cstdlib>
#include <memory>

#include "base/test/mock_callback.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/permission_type.h"
#include "content/public/test/mock_permission_manager.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {
using ::testing::Unused;
using RequestsCallback =
    base::Callback<void(const std::vector<blink::mojom::PermissionStatus>&)>;

constexpr char kTestUrl[] = "https://google.com";

class MockManagerWithRequests : public MockPermissionManager {
 public:
  MockManagerWithRequests() {}
  ~MockManagerWithRequests() override {}
  MOCK_METHOD5(
      RequestPermissions,
      int(const std::vector<PermissionType>& permission,
          RenderFrameHost* render_frame_host,
          const GURL& requesting_origin,
          bool user_gesture,
          const base::OnceCallback<void(
              const std::vector<blink::mojom::PermissionStatus>&)> callback));
  MOCK_METHOD2(SetPermissionOverridesForDevTools,
               void(const GURL& origin, const PermissionOverrides& overrides));
  MOCK_METHOD0(ResetPermissionOverridesForDevTools, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockManagerWithRequests);
};

class PermissionControllerImplTest : public ::testing::Test {
 public:
  PermissionControllerImplTest() {
    browser_context_.SetPermissionControllerDelegate(
        std::make_unique<::testing::NiceMock<MockManagerWithRequests>>());
    permission_controller_ =
        std::make_unique<PermissionControllerImpl>(&browser_context_);
  }
  ~PermissionControllerImplTest() override {}

  PermissionControllerImpl* permission_controller() {
    return permission_controller_.get();
  }

  BrowserContext* browser_context() { return &browser_context_; }

  MockManagerWithRequests* mock_manager() {
    return static_cast<MockManagerWithRequests*>(
        browser_context_.GetPermissionControllerDelegate());
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestBrowserContext browser_context_;
  std::unique_ptr<PermissionControllerImpl> permission_controller_;

  DISALLOW_COPY_AND_ASSIGN(PermissionControllerImplTest);
};

TEST_F(PermissionControllerImplTest, ResettingOverridesForwardsReset) {
  EXPECT_CALL(*mock_manager(), ResetPermissionOverridesForDevTools());
  permission_controller()->ResetOverridesForDevTools();
}

TEST_F(PermissionControllerImplTest, SettingOverridesForwardsUpdates) {
  GURL kTestOrigin(kTestUrl);
  EXPECT_CALL(*mock_manager(),
              SetPermissionOverridesForDevTools(
                  kTestOrigin, testing::ElementsAre(testing::Pair(
                                   PermissionType::GEOLOCATION,
                                   blink::mojom::PermissionStatus::GRANTED))));
  permission_controller()->SetOverrideForDevTools(
      kTestOrigin, PermissionType::GEOLOCATION,
      blink::mojom::PermissionStatus::GRANTED);
}

TEST_F(PermissionControllerImplTest,
       RequestPermissionsDelegatesIffMissingOverrides) {
  GURL kTestOrigin(kTestUrl);
  RenderViewHostTestEnabler enabler;

  const std::vector<PermissionType> kTypesToQuery = {
      PermissionType::GEOLOCATION, PermissionType::BACKGROUND_SYNC,
      PermissionType::MIDI_SYSEX};

  // Results are defined based on assumption that same types are queried for
  // each test case.
  const struct {
    std::map<PermissionType, blink::mojom::PermissionStatus> overrides;

    std::vector<PermissionType> delegated_permissions;
    std::vector<blink::mojom::PermissionStatus> delegated_statuses;

    std::vector<blink::mojom::PermissionStatus> expected_results;
    bool expect_death;
  } kTestCases[] = {
      // No overrides present - all delegated.
      {{},
       {PermissionType::GEOLOCATION, PermissionType::BACKGROUND_SYNC,
        PermissionType::MIDI_SYSEX},
       {blink::mojom::PermissionStatus::DENIED,
        blink::mojom::PermissionStatus::GRANTED,
        blink::mojom::PermissionStatus::GRANTED},
       {blink::mojom::PermissionStatus::DENIED,
        blink::mojom::PermissionStatus::GRANTED,
        blink::mojom::PermissionStatus::GRANTED},
       /*expect_death=*/false},

      // No delegates needed - all overridden.
      {{{PermissionType::GEOLOCATION, blink::mojom::PermissionStatus::GRANTED},
        {PermissionType::BACKGROUND_SYNC,
         blink::mojom::PermissionStatus::GRANTED},
        {PermissionType::MIDI_SYSEX, blink::mojom::PermissionStatus::ASK}},
       {},
       {},
       {blink::mojom::PermissionStatus::GRANTED,
        blink::mojom::PermissionStatus::GRANTED,
        blink::mojom::PermissionStatus::ASK},
       /*expect_death=*/false},

      // Some overridden, some delegated.
      {{{PermissionType::BACKGROUND_SYNC,
         blink::mojom::PermissionStatus::DENIED}},
       {PermissionType::GEOLOCATION, PermissionType::MIDI_SYSEX},
       {blink::mojom::PermissionStatus::GRANTED,
        blink::mojom::PermissionStatus::ASK},
       {blink::mojom::PermissionStatus::GRANTED,
        blink::mojom::PermissionStatus::DENIED,
        blink::mojom::PermissionStatus::ASK},
       /*expect_death=*/false},

      // Some overridden, some delegated.
      {{{PermissionType::GEOLOCATION, blink::mojom::PermissionStatus::GRANTED},
        {PermissionType::BACKGROUND_SYNC,
         blink::mojom::PermissionStatus::DENIED}},
       {PermissionType::MIDI_SYSEX},
       {blink::mojom::PermissionStatus::ASK},
       {blink::mojom::PermissionStatus::GRANTED,
        blink::mojom::PermissionStatus::DENIED,
        blink::mojom::PermissionStatus::ASK},
       /*expect_death=*/false},

      // Too many delegates (causes death).
      {{{PermissionType::GEOLOCATION, blink::mojom::PermissionStatus::GRANTED},
        {PermissionType::MIDI_SYSEX, blink::mojom::PermissionStatus::ASK}},
       {PermissionType::BACKGROUND_SYNC},
       {blink::mojom::PermissionStatus::DENIED,
        blink::mojom::PermissionStatus::GRANTED},
       // Results don't matter because will die.
       {},
       /*expect_death=*/true},

      // Too few delegates (causes death).
      {{},
       {PermissionType::GEOLOCATION, PermissionType::BACKGROUND_SYNC,
        PermissionType::MIDI_SYSEX},
       {blink::mojom::PermissionStatus::GRANTED,
        blink::mojom::PermissionStatus::GRANTED},
       // Results don't matter because will die.
       {},
       /*expect_death=*/true}};

  auto web_contents = base::WrapUnique(WebContentsTester::CreateTestWebContents(
      WebContents::CreateParams(browser_context(), nullptr)));
  RenderFrameHost* rfh = web_contents->GetMainFrame();
  for (const auto& test_case : kTestCases) {
    // Need to reset overrides for each case to ensure delegation is as
    // expected.
    permission_controller()->ResetOverridesForDevTools();
    for (const auto& permission_status_pair : test_case.overrides) {
      permission_controller()->SetOverrideForDevTools(
          kTestOrigin, permission_status_pair.first,
          permission_status_pair.second);
    }

    // Expect request permission call if override are missing.
    if (!test_case.delegated_permissions.empty()) {
      auto forward_callbacks = testing::WithArg<4>(
          [&test_case](base::OnceCallback<void(
                           const std::vector<blink::mojom::PermissionStatus>&)>
                           callback) {
            std::move(callback).Run(test_case.delegated_statuses);
            return 0;
          });
      // Regular tests can set expectations.
      if (!test_case.expect_death) {
        EXPECT_CALL(*mock_manager(),
                    RequestPermissions(testing::ElementsAreArray(
                                           test_case.delegated_permissions),
                                       rfh, kTestOrigin, true, testing::_))
            .WillOnce(testing::Invoke(forward_callbacks));
      } else {
        // Death tests cannot track these expectations but arguments should be
        // forwarded to ensure death occurs.
        ON_CALL(*mock_manager(),
                RequestPermissions(
                    testing::ElementsAreArray(test_case.delegated_permissions),
                    rfh, kTestOrigin, true, testing::_))
            .WillByDefault(testing::Invoke(forward_callbacks));
      }
    } else {
      // There should be no call to delegate if all overrides are defined.
      EXPECT_CALL(*mock_manager(), RequestPermissions).Times(0);
    }
    if (!test_case.expect_death) {
      base::MockCallback<RequestsCallback> callback;
      EXPECT_CALL(callback,
                  Run(testing::ElementsAreArray(test_case.expected_results)));
      permission_controller()->RequestPermissions(
          kTypesToQuery, rfh, kTestOrigin,
          /*user_gesture=*/true, callback.Get());
    } else {
      ::testing::FLAGS_gtest_death_test_style = "threadsafe";
      base::MockCallback<RequestsCallback> callback;
      EXPECT_DEATH_IF_SUPPORTED(permission_controller()->RequestPermissions(
                                    kTypesToQuery, rfh, kTestOrigin,
                                    /*user_gesture=*/true, callback.Get()),
                                "");
    }
  }
}

TEST_F(PermissionControllerImplTest,
       GetPermissionStatusDelegatesIffNoOverrides) {
  GURL kTestOrigin(kTestUrl);
  EXPECT_CALL(*mock_manager(), GetPermissionStatus(PermissionType::GEOLOCATION,
                                                   kTestOrigin, kTestOrigin))
      .WillOnce(testing::Return(blink::mojom::PermissionStatus::DENIED));

  blink::mojom::PermissionStatus status =
      permission_controller()->GetPermissionStatus(PermissionType::GEOLOCATION,
                                                   kTestOrigin, kTestOrigin);
  EXPECT_EQ(status, blink::mojom::PermissionStatus::DENIED);

  permission_controller()->SetOverrideForDevTools(
      kTestOrigin, PermissionType::GEOLOCATION,
      blink::mojom::PermissionStatus::GRANTED);
  EXPECT_CALL(*mock_manager(), GetPermissionStatus(PermissionType::GEOLOCATION,
                                                   kTestOrigin, kTestOrigin))
      .Times(0);
  status = permission_controller()->GetPermissionStatus(
      PermissionType::GEOLOCATION, kTestOrigin, kTestOrigin);
  EXPECT_EQ(status, blink::mojom::PermissionStatus::GRANTED);
}

TEST_F(PermissionControllerImplTest,
       GetPermissionStatusForFrameDelegatesIffNoOverrides) {
  GURL kTestOrigin(kTestUrl);
  EXPECT_CALL(*mock_manager(),
              GetPermissionStatusForFrame(PermissionType::GEOLOCATION, nullptr,
                                          kTestOrigin))
      .WillOnce(testing::Return(blink::mojom::PermissionStatus::DENIED));

  blink::mojom::PermissionStatus status =
      permission_controller()->GetPermissionStatusForFrame(
          PermissionType::GEOLOCATION, nullptr, kTestOrigin);
  EXPECT_EQ(status, blink::mojom::PermissionStatus::DENIED);

  permission_controller()->SetOverrideForDevTools(
      kTestOrigin, PermissionType::GEOLOCATION,
      blink::mojom::PermissionStatus::GRANTED);
  EXPECT_CALL(*mock_manager(),
              GetPermissionStatusForFrame(PermissionType::GEOLOCATION, nullptr,
                                          kTestOrigin))
      .Times(0);
  status = permission_controller()->GetPermissionStatusForFrame(
      PermissionType::GEOLOCATION, nullptr, kTestOrigin);
  EXPECT_EQ(status, blink::mojom::PermissionStatus::GRANTED);
}

TEST_F(PermissionControllerImplTest,
       NotifyChangedSubscriptionsCallsOnChangeOnly) {
  using PermissionStatusCallback =
      base::Callback<void(blink::mojom::PermissionStatus)>;
  GURL kTestOrigin(kTestUrl);

  // Setup.
  blink::mojom::PermissionStatus sync_status =
      permission_controller()->GetPermissionStatus(
          PermissionType::BACKGROUND_SYNC, kTestOrigin, kTestOrigin);
  permission_controller()->SetOverrideForDevTools(
      kTestOrigin, PermissionType::GEOLOCATION,
      blink::mojom::PermissionStatus::DENIED);

  base::MockCallback<PermissionStatusCallback> geo_callback;
  permission_controller()->SubscribePermissionStatusChange(
      PermissionType::GEOLOCATION, nullptr, kTestOrigin, geo_callback.Get());

  base::MockCallback<PermissionStatusCallback> sync_callback;
  permission_controller()->SubscribePermissionStatusChange(
      PermissionType::BACKGROUND_SYNC, nullptr, kTestOrigin,
      sync_callback.Get());

  // Geolocation should change status, so subscriber is updated.
  EXPECT_CALL(geo_callback, Run(blink::mojom::PermissionStatus::ASK));
  EXPECT_CALL(sync_callback, Run).Times(0);
  permission_controller()->SetOverrideForDevTools(
      kTestOrigin, PermissionType::GEOLOCATION,
      blink::mojom::PermissionStatus::ASK);

  // Callbacks should not be called again because permission status has not
  // changed.
  permission_controller()->SetOverrideForDevTools(
      kTestOrigin, PermissionType::BACKGROUND_SYNC, sync_status);
  permission_controller()->SetOverrideForDevTools(
      kTestOrigin, PermissionType::GEOLOCATION,
      blink::mojom::PermissionStatus::ASK);
}

}  // namespace

}  // namespace content
