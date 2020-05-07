// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_recovery_view.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_base.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_test_helper.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/guest_os/guest_os_registry_service.h"
#include "chrome/browser/chromeos/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/crostini/crostini_browser_test_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

constexpr crostini::CrostiniUISurface kUiSurface =
    crostini::CrostiniUISurface::kAppList;
constexpr char kDesktopFileId[] = "test_app";
constexpr int kDisplayId = 0;

class CrostiniRecoveryViewBrowserTest : public CrostiniDialogBrowserTest {
 public:
  CrostiniRecoveryViewBrowserTest()
      : CrostiniDialogBrowserTest(true /*register_termina*/),
        app_id_(crostini::CrostiniTestHelper::GenerateAppId(kDesktopFileId)) {}

  void SetUpOnMainThread() override {
    CrostiniDialogBrowserTest::SetUpOnMainThread();

    static_cast<chromeos::FakeConciergeClient*>(
        chromeos::DBusThreadManager::Get()->GetConciergeClient())
        ->set_notify_vm_stopped_on_stop_vm(true);
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    ShowCrostiniRecoveryView(browser()->profile(), kUiSurface, app_id(),
                             kDisplayId, base::DoNothing());
  }

  void SetUncleanStartup() {
    crostini::CrostiniManager::GetForProfile(browser()->profile())
        ->SetUncleanStartupForTesting(true);
  }

  CrostiniRecoveryView* ActiveView() {
    return CrostiniRecoveryView::GetActiveViewForTesting();
  }

  bool HasAcceptButton() { return ActiveView()->GetOkButton() != nullptr; }

  bool HasCancelButton() { return ActiveView()->GetCancelButton() != nullptr; }

  void WaitForViewDestroyed() {
    base::RunLoop().RunUntilIdle();
    ExpectNoView();
  }

  void ExpectView() {
    // A new Widget was created in ShowUi() or since the last VerifyUi().
    EXPECT_TRUE(VerifyUi());
    // There is one view, and it's ours.
    EXPECT_NE(nullptr, ActiveView());
    EXPECT_EQ(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL,
              ActiveView()->GetDialogButtons());

    EXPECT_TRUE(HasAcceptButton());
    EXPECT_TRUE(HasCancelButton());
  }

  void ExpectNoView() {
    // No new Widget was created in ShowUi() or since the last VerifyUi().
    EXPECT_FALSE(VerifyUi());
    // Our view has really been deleted.
    EXPECT_EQ(nullptr, ActiveView());
  }

  bool IsUncleanStartup() {
    return crostini::CrostiniManager::GetForProfile(browser()->profile())
        ->IsUncleanStartup();
  }

  void RegisterApp() {
    vm_tools::apps::ApplicationList app_list =
        crostini::CrostiniTestHelper::BasicAppList(
            kDesktopFileId, crostini::kCrostiniDefaultVmName,
            crostini::kCrostiniDefaultContainerName);
    guest_os::GuestOsRegistryServiceFactory::GetForProfile(browser()->profile())
        ->UpdateApplicationList(app_list);
  }

  const std::string& app_id() const { return app_id_; }

 private:
  std::string app_id_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniRecoveryViewBrowserTest);
};

// Test the dialog is actually launched.
IN_PROC_BROWSER_TEST_F(CrostiniRecoveryViewBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(CrostiniRecoveryViewBrowserTest, NoViewOnNormalStartup) {
  base::HistogramTester histogram_tester;
  RegisterApp();

  crostini::LaunchCrostiniApp(browser()->profile(), app_id(), kDisplayId);
  ExpectNoView();
}

IN_PROC_BROWSER_TEST_F(CrostiniRecoveryViewBrowserTest, ShowsOnUncleanStart) {
  base::HistogramTester histogram_tester;

  SetUncleanStartup();
  RegisterApp();

  crostini::LaunchCrostiniApp(browser()->profile(), app_id(), kDisplayId);
  ExpectView();

  ActiveView()->CancelDialog();

  WaitForViewDestroyed();
  // Canceling means we aren't restarting the VM.
  EXPECT_TRUE(IsUncleanStartup());

  histogram_tester.ExpectUniqueSample(
      "Crostini.RecoverySource",
      static_cast<base::HistogramBase::Sample>(kUiSurface), 1);
}

IN_PROC_BROWSER_TEST_F(CrostiniRecoveryViewBrowserTest,
                       ReshowViewIfRestartNotSelected) {
  base::HistogramTester histogram_tester;

  SetUncleanStartup();
  RegisterApp();

  crostini::LaunchCrostiniApp(browser()->profile(), app_id(), kDisplayId);
  ExpectView();

  ActiveView()->CancelDialog();
  WaitForViewDestroyed();

  crostini::LaunchCrostiniApp(browser()->profile(), app_id(), kDisplayId);
  ExpectView();

  ActiveView()->CancelDialog();
  WaitForViewDestroyed();

  EXPECT_TRUE(IsUncleanStartup());

  histogram_tester.ExpectUniqueSample(
      "Crostini.RecoverySource",
      static_cast<base::HistogramBase::Sample>(kUiSurface), 2);
}

IN_PROC_BROWSER_TEST_F(CrostiniRecoveryViewBrowserTest, NoViewAfterRestart) {
  base::HistogramTester histogram_tester;

  SetUncleanStartup();
  RegisterApp();

  crostini::LaunchCrostiniApp(browser()->profile(), app_id(), kDisplayId);
  ExpectView();

  ActiveView()->AcceptDialog();

  WaitForViewDestroyed();

  EXPECT_FALSE(IsUncleanStartup());

  crostini::LaunchCrostiniApp(browser()->profile(), app_id(), kDisplayId);
  ExpectNoView();

  histogram_tester.ExpectUniqueSample(
      "Crostini.RecoverySource",
      static_cast<base::HistogramBase::Sample>(kUiSurface), 1);
}
