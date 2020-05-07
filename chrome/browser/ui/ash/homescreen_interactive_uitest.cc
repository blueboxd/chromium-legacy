// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/perf/drag_event_generator.h"
#include "chrome/test/base/perf/performance_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
#include "ui/base/test/ui_controls.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/wm/core/wm_core_switches.h"

class HomescreenTest : public UIPerformanceTest {
 public:
  HomescreenTest() = default;
  ~HomescreenTest() override = default;

  HomescreenTest(const HomescreenTest& other) = delete;
  HomescreenTest& operator=(const HomescreenTest& rhs) = delete;

  // UIPerformanceTest:
  void SetUpOnMainThread() override {
    UIPerformanceTest::SetUpOnMainThread();
    test::PopulateDummyAppListItems(100);
    ash::ShellTestApi().SetTabletModeEnabledForTest(true);

    // Make sure startup tasks won't affect measurement.
    if (base::SysInfo::IsRunningOnChromeOS()) {
      base::RunLoop run_loop;
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, run_loop.QuitClosure(), base::TimeDelta::FromSeconds(5));
      run_loop.Run();
    }

    // The test will wait for the browser window to finish animating to know
    // when to continue, so make sure the window has animations.
    auto* cmd = base::CommandLine::ForCurrentProcess();
    if (cmd->HasSwitch(wm::switches::kWindowAnimationsDisabled))
      cmd->RemoveSwitch(wm::switches::kWindowAnimationsDisabled);
  }
  std::vector<std::string> GetUMAHistogramNames() const override {
    return {"Ash.Homescreen.AnimationSmoothness"};
  }
};

IN_PROC_BROWSER_TEST_F(HomescreenTest, ShowHideLauncher) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  aura::Window* browser_window = browser_view->GetWidget()->GetNativeWindow();

  // Showing launcher might minimize the active window, which recreates the
  // window layer (while the original layer continues animating) - set up
  // animation waiter on the original window layer, before minimization starts.
  base::OnceClosure waiter =
      ash::ShellTestApi().CreateWaiterForFinishingWindowAnimation(
          browser_window);

  // Shows launcher using accelerator.
  ui_controls::SendKeyPress(browser_window, ui::VKEY_BROWSER_SEARCH,
                            /*control=*/false,
                            /*shift=*/false,
                            /*alt=*/false,
                            /*command=*/false);
  base::RunLoop().RunUntilIdle();
  std::move(waiter).Run();

  // Hide the launcher by activating the browser window.
  waiter = ash::ShellTestApi().CreateWaiterForFinishingWindowAnimation(
      browser_window);
  ui_controls::SendKeyPress(browser_window, ui::VKEY_1,
                            /*control=*/false,
                            /*shift=*/false,
                            /*alt=*/true,
                            /*command=*/false);
  base::RunLoop().RunUntilIdle();
  std::move(waiter).Run();
}
