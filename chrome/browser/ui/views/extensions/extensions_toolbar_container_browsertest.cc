// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_browsertest.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"

class ExtensionsToolbarContainerBrowserTest
    : public ExtensionsToolbarBrowserTest {
 public:
  ExtensionsToolbarContainerBrowserTest() = default;
  ExtensionsToolbarContainerBrowserTest(
      const ExtensionsToolbarContainerBrowserTest&) = delete;
  ExtensionsToolbarContainerBrowserTest& operator=(
      const ExtensionsToolbarContainerBrowserTest&) = delete;
  ~ExtensionsToolbarContainerBrowserTest() override = default;

  void ClickOnAction(ToolbarActionView* action) {
    ui::MouseEvent click_down_event(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                    gfx::Point(), base::TimeTicks(),
                                    ui::EF_LEFT_MOUSE_BUTTON, 0);
    ui::MouseEvent click_up_event(ui::ET_MOUSE_RELEASED, gfx::Point(),
                                  gfx::Point(), base::TimeTicks(),
                                  ui::EF_LEFT_MOUSE_BUTTON, 0);
    action->OnMouseEvent(&click_down_event);
    action->OnMouseEvent(&click_up_event);
  }

  void ShowUi(const std::string& name) override { NOTREACHED(); }
};

// TODO(devlin): There are probably some tests from
// ExtensionsMenuViewBrowserTest that should move here (if they test the
// toolbar container more than the menu).

// Tests that invocation metrics are properly recorded when triggering
// extensions from the toolbar.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerBrowserTest,
                       InvocationMetrics) {
  base::HistogramTester histogram_tester;
  scoped_refptr<const extensions::Extension> extension =
      LoadTestExtension("extensions/uitest/extension_with_action_and_command");

  EXPECT_EQ(1u, GetToolbarActionViews().size());
  EXPECT_EQ(0u, GetVisibleToolbarActionViews().size());

  ToolbarActionsModel* const model = ToolbarActionsModel::Get(profile());
  model->SetActionVisibility(extension->id(), true);

  auto* container = GetExtensionsToolbarContainer();
  container->GetWidget()->LayoutRootViewIfNecessary();

  ASSERT_EQ(1u, GetVisibleToolbarActionViews().size());
  ToolbarActionView* const action = GetVisibleToolbarActionViews()[0];

  constexpr char kHistogramName[] = "Extensions.Toolbar.InvocationSource";
  histogram_tester.ExpectTotalCount(kHistogramName, 0);

  // Trigger the extension by clicking on it.
  ClickOnAction(action);

  histogram_tester.ExpectTotalCount(kHistogramName, 1);
  histogram_tester.ExpectBucketCount(
      kHistogramName,
      ToolbarActionViewController::InvocationSource::kToolbarButton, 1);
}
