// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_access_manager_impl.h"

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/public/test/mock_permission_manager.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_frame_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using PermissionCallback =
    base::OnceCallback<void(blink::mojom::PermissionStatus)>;

class TestPermissionManager : public MockPermissionManager {
 public:
  TestPermissionManager() = default;
  ~TestPermissionManager() override = default;

  int RequestPermission(PermissionType permissions,
                        RenderFrameHost* render_frame_host,
                        const GURL& requesting_origin,
                        bool user_gesture,
                        PermissionCallback callback) override {
    EXPECT_EQ(permissions, PermissionType::FONT_ACCESS);
    EXPECT_TRUE(user_gesture);
    request_callback_.Run(std::move(callback));
    return 0;
  }

  void SetRequestCallback(
      base::RepeatingCallback<void(PermissionCallback)> request_callback) {
    request_callback_ = std::move(request_callback);
  }

 private:
  base::RepeatingCallback<void(PermissionCallback)> request_callback_;
};

}  // namespace

class FontAccessManagerImplTest : public RenderViewHostImplTestHarness {
 public:
  FontAccessManagerImplTest() {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kFontAccess);
  }

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    NavigateAndCommit(kTestUrl);

    const int process_id = main_rfh()->GetProcess()->GetID();
    const int routing_id = main_rfh()->GetRoutingID();
    const GlobalFrameRoutingId frame_id =
        GlobalFrameRoutingId{process_id, routing_id};
    const FontAccessManagerImpl::BindingContext bindingContext = {kTestOrigin,
                                                                  frame_id};

    manager_ = std::make_unique<FontAccessManagerImpl>();
    manager_->BindReceiver(bindingContext,
                           manager_remote_.BindNewPipeAndPassReceiver());

    // Set up permission mock.
    TestBrowserContext* browser_context =
        static_cast<TestBrowserContext*>(main_rfh()->GetBrowserContext());
    browser_context->SetPermissionControllerDelegate(
        std::make_unique<TestPermissionManager>());
    permission_controller_ =
        std::make_unique<PermissionControllerImpl>(browser_context);
  }

  void TearDown() override { RenderViewHostImplTestHarness::TearDown(); }

  TestPermissionManager* test_permission_manager() {
    return static_cast<TestPermissionManager*>(
        main_rfh()->GetBrowserContext()->GetPermissionControllerDelegate());
  }

 protected:
  const GURL kTestUrl = GURL("https://example.com/font_access");
  const url::Origin kTestOrigin = url::Origin::Create(GURL(kTestUrl));

  std::unique_ptr<FontAccessManagerImpl> manager_;
  mojo::Remote<blink::mojom::FontAccessManager> manager_remote_;

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<PermissionControllerImpl> permission_controller_;
};

TEST_F(FontAccessManagerImplTest, NoUserActivationPermissionDenied) {
  ASSERT_TRUE(manager_remote_.is_bound() && manager_remote_.is_connected());

  base::RunLoop loop;
  bool permission_requested = false;
  manager_remote_->RequestPermission(
      base::BindLambdaForTesting([&](blink::mojom::PermissionStatus status) {
        permission_requested = true;
        EXPECT_EQ(blink::mojom::PermissionStatus::DENIED, status)
            << "No user activation yields a permission denied status";
        loop.Quit();
      }));

  loop.Run();
  EXPECT_TRUE(permission_requested) << "Permission has been requested";
}

TEST_F(FontAccessManagerImplTest, UserActivationPermissionManagerTriggered) {
  ASSERT_TRUE(manager_remote_.is_bound() && manager_remote_.is_connected());

  // Auto-grant permissions for test.
  test_permission_manager()->SetRequestCallback(
      base::BindRepeating([](PermissionCallback callback) {
        std::move(callback).Run(blink::mojom::PermissionStatus::GRANTED);
      }));

  // Simulate user activation.
  static_cast<RenderFrameHostImpl*>(main_rfh())
      ->UpdateUserActivationState(
          blink::mojom::UserActivationUpdateType::kNotifyActivation,
          blink::mojom::UserActivationNotificationType::kInteraction);

  base::RunLoop loop;
  bool permission_requested = false;
  manager_remote_->RequestPermission(
      base::BindLambdaForTesting([&](blink::mojom::PermissionStatus status) {
        permission_requested = true;
        EXPECT_EQ(blink::mojom::PermissionStatus::GRANTED, status)
            << "User activation yields a permission granted status";
        loop.Quit();
      }));

  loop.Run();
  EXPECT_TRUE(permission_requested) << "Permission has been requested";
}

}  // namespace content
