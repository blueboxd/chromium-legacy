// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/system_proxy_manager.h"
#include "chrome/browser/chromeos/ui/request_system_proxy_credentials_view.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/dbus/system_proxy/system_proxy_client.h"
#include "chromeos/dbus/system_proxy/system_proxy_service.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"

namespace policy {

namespace {
constexpr char kRealm[] = "My proxy";
constexpr char kScheme[] = "dIgEsT";
constexpr char kProxyAuthUrl[] = "http://example.com:3128";
constexpr char kSystemProxyNotificationId[] = "system-proxy.auth_required";
constexpr char kUsername[] = "testuser";
constexpr char kPassword[] = "testpwd";
}  // namespace

class SystemProxyManagerBrowserTest : public InProcessBrowserTest {
 public:
  SystemProxyManagerBrowserTest() = default;
  SystemProxyManagerBrowserTest(const SystemProxyManagerBrowserTest&) = delete;
  SystemProxyManagerBrowserTest& operator=(
      const SystemProxyManagerBrowserTest&) = delete;
  ~SystemProxyManagerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    GetSystemProxyManager()->StartObservingPrimaryProfilePrefs(
        browser()->profile());
    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(/*profile=*/nullptr);
    GetSystemProxyManager()->SetSystemProxyEnabledForTest(true);
  }

  void TearDownOnMainThread() override {
    GetSystemProxyManager()->SetSystemProxyEnabledForTest(false);
  }

 protected:
  SystemProxyManager* GetSystemProxyManager() {
    return g_browser_process->platform_part()
        ->browser_policy_connector_chromeos()
        ->GetSystemProxyManager();
  }

  chromeos::RequestSystemProxyCredentialsView* dialog() {
    return GetSystemProxyManager()->GetActiveAuthDialogForTest();
  }

  chromeos::SystemProxyClient::TestInterface* client_test_interface() {
    return chromeos::SystemProxyClient::Get()->GetTestInterface();
  }

  void SendAuthenticationRequest(bool bad_cached_credentials) {
    system_proxy::ProtectionSpace protection_space;
    protection_space.set_origin(kProxyAuthUrl);
    protection_space.set_scheme(kScheme);
    protection_space.set_realm(kRealm);

    system_proxy::AuthenticationRequiredDetails details;
    details.set_bad_cached_credentials(bad_cached_credentials);
    *details.mutable_proxy_protection_space() = protection_space;

    client_test_interface()->SendAuthenticationRequiredSignal(details);
  }

  void WaitForNotification() {
    base::RunLoop run_loop;
    display_service_tester_->SetNotificationAddedClosure(
        run_loop.QuitClosure());
    run_loop.Run();
  }

  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;
};

// Tests the flow for setting user credentials for System-proxy:
// - Receiving an authentication request prompts a notification;
// - Clicking on the notification opens a dialog;
// - Credentials introduced in the dialog are sent via D-Bus to System-proxy.
IN_PROC_BROWSER_TEST_F(SystemProxyManagerBrowserTest, AuthenticationDialog) {
  base::RunLoop run_loop;
  GetSystemProxyManager()->SetSendAuthDetailsClosureForTest(
      run_loop.QuitClosure());

  EXPECT_FALSE(
      display_service_tester_->GetNotification(kSystemProxyNotificationId));
  SendAuthenticationRequest(/* bad_cached_credentials = */ false);
  WaitForNotification();

  EXPECT_TRUE(
      display_service_tester_->GetNotification(kSystemProxyNotificationId));

  display_service_tester_->SimulateClick(
      NotificationHandler::Type::TRANSIENT, kSystemProxyNotificationId,
      /*action_index=*/base::nullopt, /*reply=*/base::nullopt);
  // Dialog is created.
  ASSERT_TRUE(dialog());

  // Expect warning is not shown.
  ASSERT_FALSE(dialog()->error_label_for_testing()->GetVisible());
  dialog()->username_textfield_for_testing()->SetText(
      base::ASCIIToUTF16(kUsername));
  dialog()->password_textfield_for_testing()->SetText(
      base::ASCIIToUTF16(kPassword));

  // Simulate clicking on "OK" button.
  dialog()->Accept();

  // Wait for the callback set via |SetSendAuthDetailsClosureForTest| to be
  // called. The callback will be called when SystemProxyManager calls the D-Bus
  // method |SetAuthenticationDetails|
  run_loop.Run();

  system_proxy::SetAuthenticationDetailsRequest request =
      client_test_interface()->GetLastAuthenticationDetailsRequest();

  ASSERT_TRUE(request.has_credentials());
  EXPECT_EQ(request.credentials().username(), kUsername);
  EXPECT_EQ(request.credentials().password(), kPassword);

  // Verify that the UI elements are reset.
  GetSystemProxyManager()->CloseAuthDialogForTest();
  EXPECT_FALSE(
      display_service_tester_->GetNotification(kSystemProxyNotificationId));
  EXPECT_FALSE(dialog());
}

// Tests that canceling the authentication dialog sends empty credentials to
// System-proxy.
IN_PROC_BROWSER_TEST_F(SystemProxyManagerBrowserTest,
                       CancelAuthenticationDialog) {
  EXPECT_FALSE(
      display_service_tester_->GetNotification(kSystemProxyNotificationId));
  SendAuthenticationRequest(/* bad_cached_credentials = */ false);
  WaitForNotification();
  EXPECT_TRUE(
      display_service_tester_->GetNotification(kSystemProxyNotificationId));

  display_service_tester_->SimulateClick(
      NotificationHandler::Type::TRANSIENT, kSystemProxyNotificationId,
      /*action_index=*/base::nullopt, /*reply=*/base::nullopt);

  // Dialog is created.
  ASSERT_TRUE(dialog());

  // Expect warning is not shown.
  ASSERT_FALSE(dialog()->error_label_for_testing()->GetVisible());
  dialog()->username_textfield_for_testing()->SetText(
      base::ASCIIToUTF16(kUsername));
  dialog()->password_textfield_for_testing()->SetText(
      base::ASCIIToUTF16(kPassword));

  base::RunLoop run_loop;
  GetSystemProxyManager()->SetSendAuthDetailsClosureForTest(
      run_loop.QuitClosure());
  // Simulate clicking on "Cancel" button.
  dialog()->Cancel();
  run_loop.Run();

  system_proxy::SetAuthenticationDetailsRequest request =
      client_test_interface()->GetLastAuthenticationDetailsRequest();

  ASSERT_TRUE(request.has_credentials());
  EXPECT_EQ(request.credentials().username(), "");
  EXPECT_EQ(request.credentials().password(), "");

  // Verify that the UI elements are reset.
  GetSystemProxyManager()->CloseAuthDialogForTest();
  EXPECT_FALSE(
      display_service_tester_->GetNotification(kSystemProxyNotificationId));
  EXPECT_FALSE(dialog());
}

// Tests that the warning informing the user that the previous credentials are
// incorrect is shown in the UI.
IN_PROC_BROWSER_TEST_F(SystemProxyManagerBrowserTest,
                       BadCachedCredentialsWarning) {
  EXPECT_FALSE(
      display_service_tester_->GetNotification(kSystemProxyNotificationId));
  SendAuthenticationRequest(/* bad_cached_credentials = */ true);
  WaitForNotification();
  EXPECT_TRUE(
      display_service_tester_->GetNotification(kSystemProxyNotificationId));

  display_service_tester_->SimulateClick(
      NotificationHandler::Type::TRANSIENT, kSystemProxyNotificationId,
      /*action_index=*/base::nullopt, /*reply=*/base::nullopt);
  ASSERT_TRUE(dialog());

  // Expect warning is shown.
  EXPECT_TRUE(dialog()->error_label_for_testing()->GetVisible());
}

}  // namespace policy
