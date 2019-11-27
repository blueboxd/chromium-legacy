// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/perf/drag_event_generator.h"
#include "chrome/test/base/perf/performance_test.h"
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
      base::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                            base::TimeDelta::FromSeconds(5));
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

  // Shows launcher using accelerator.
  ui_controls::SendKeyPress(browser_window, ui::VKEY_BROWSER_SEARCH,
                            /*control=*/false,
                            /*shift=*/false,
                            /*alt=*/false,
                            /*command=*/false);
  base::RunLoop().RunUntilIdle();
  ash::ShellTestApi().WaitForWindowFinishAnimating(browser_window);

  // Hide the launcher by activating the browser window.
  ui_controls::SendKeyPress(browser_window, ui::VKEY_1,
                            /*control=*/false,
                            /*shift=*/false,
                            /*alt=*/true,
                            /*command=*/false);
  base::RunLoop().RunUntilIdle();
  ash::ShellTestApi().WaitForWindowFinishAnimating(browser_window);
}

class HomescreenDragTest : public HomescreenTest {
 public:
  HomescreenDragTest() {
    // Gesture tested by this test is only enabled if
    // kDragFromShelfToHomeOrOverview is disabled.
    scoped_features_.InitAndDisableFeature(
        ash::features::kDragFromShelfToHomeOrOverview);
  }
  ~HomescreenDragTest() override = default;

  HomescreenDragTest(const HomescreenDragTest& other) = delete;
  HomescreenDragTest& operator=(const HomescreenDragTest& rhs) = delete;

  std::vector<std::string> GetUMAHistogramNames() const override {
    return {
        "Ash.Homescreen.AnimationSmoothness",
        "Apps.StateTransition.Drag.PresentationTime.TabletMode",
        "Apps.StateTransition.Drag.PresentationTime.MaxLatency.TabletMode",
    };
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

IN_PROC_BROWSER_TEST_F(HomescreenDragTest, DraggingPerformance) {
  // First show the launcher so we can do drags.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  aura::Window* browser_window = browser_view->GetWidget()->GetNativeWindow();
  ui_controls::SendKeyPress(browser_window, ui::VKEY_BROWSER_SEARCH,
                            /*control=*/false,
                            /*shift=*/false,
                            /*alt=*/false,
                            /*command=*/false);
  base::RunLoop().RunUntilIdle();
  ash::ShellTestApi().WaitForWindowFinishAnimating(browser_window);

  // Drag down to somewhere above halfway, so the launcher remains shown on drag
  // release.
  gfx::Rect display_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(browser()->window()->GetNativeWindow())
          .bounds();
  const gfx::Point start_point(display_bounds.CenterPoint().x(), 1);
  gfx::Point end_point(display_bounds.CenterPoint().x(),
                       display_bounds.CenterPoint().y() - 50);
  auto generator = ui_test_utils::DragEventGenerator::CreateForTouch(
      std::make_unique<ui_test_utils::InterpolatedProducer>(
          start_point, end_point, base::TimeDelta::FromMilliseconds(1000)));
  generator->Wait();
  ash::ShellTestApi().WaitForWindowFinishAnimating(browser_window);

  // Drag down to somewhere below halfway, so the launcher is hidden on drag
  // release.
  end_point.set_y(display_bounds.CenterPoint().y() + 50);
  generator = ui_test_utils::DragEventGenerator::CreateForTouch(
      std::make_unique<ui_test_utils::InterpolatedProducer>(
          start_point, end_point, base::TimeDelta::FromMilliseconds(1000)));
  generator->Wait();
  ash::ShellTestApi().WaitForWindowFinishAnimating(browser_window);
}
