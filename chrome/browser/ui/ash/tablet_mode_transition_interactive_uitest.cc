// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/shell_test_api.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/perf/performance_test.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/core/wm_core_switches.h"

class TabletModeTransitionTest : public UIPerformanceTest {
 public:
  TabletModeTransitionTest() = default;
  ~TabletModeTransitionTest() override = default;

  // UIPerformanceTest:
  void SetUpOnMainThread() override {
    UIPerformanceTest::SetUpOnMainThread();

    // Eight windows is the number we want to get a good performance on.
    constexpr int additional_browsers = 7;
    constexpr int cost_per_browser = 100;
    for (int i = 0; i < additional_browsers; ++i)
      CreateBrowser(browser()->profile());

    int wait_ms = (base::SysInfo::IsRunningOnChromeOS() ? 5000 : 0) +
                  (additional_browsers + 1) * cost_per_browser;
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromMilliseconds(wait_ms));
    run_loop.Run();

    // The uma stats we are interested in measure the animations directly so we
    // need to ensure they are turned on.
    auto* cmd = base::CommandLine::ForCurrentProcess();
    if (cmd->HasSwitch(wm::switches::kWindowAnimationsDisabled))
      cmd->RemoveSwitch(wm::switches::kWindowAnimationsDisabled);

    ash::ShellTestApi::SetTabletControllerUseScreenshotForTest(true);
  }

  std::vector<std::string> GetUMAHistogramNames() const override {
    return {
        "Ash.TabletMode.AnimationSmoothness.Enter",
        "Ash.TabletMode.AnimationSmoothness.Exit",
    };
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TabletModeTransitionTest);
};

// Flaky possibly due to https://crbug.com/1054489
// TODO(sammiequon, mukai): re-enable this. See also https://crbug.com/1057868
IN_PROC_BROWSER_TEST_F(TabletModeTransitionTest, DISABLED_EnterExit) {
  // Activate the first window. The top window is the only window which animates
  // and is the one we should check to see if the tablet animation has finished.
  Browser* browser = BrowserList::GetInstance()->GetLastActive();
  aura::Window* browser_window = browser->window()->GetNativeWindow();
  ash::ShellTestApi shell_test_api;

  auto waiter =
      shell_test_api.CreateWaiterForFinishingWindowAnimation(browser_window);
  shell_test_api.SetTabletModeEnabledForTest(true,
                                             /*wait_for_completion=*/false);
  std::move(waiter).Run();

  waiter =
      shell_test_api.CreateWaiterForFinishingWindowAnimation(browser_window);
  shell_test_api.SetTabletModeEnabledForTest(false,
                                             /*wait_for_completion=*/false);
  std::move(waiter).Run();
}
