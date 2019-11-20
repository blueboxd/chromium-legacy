// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/button/image_button.h"

class ExtensionsMenuViewUnitTest : public TestWithBrowserView {
 public:
  ExtensionsMenuViewUnitTest()
      : allow_extension_menu_instances_(
            ExtensionsMenuView::AllowInstancesForTesting()) {
    feature_list_.InitAndEnableFeature(features::kExtensionsToolbarMenu);
  }
  ~ExtensionsMenuViewUnitTest() override = default;

  // TestWithBrowserView:
  void SetUp() override;

  // Adds a simple extension to the profile.
  scoped_refptr<const extensions::Extension> AddSimpleExtension(
      const char* name);

  extensions::ExtensionService* extension_service() {
    return extension_service_;
  }

  ExtensionsToolbarContainer* extensions_container() {
    return browser_view()->toolbar()->extensions_container();
  }

  ExtensionsMenuView* extensions_menu() {
    return ExtensionsMenuView::GetExtensionsMenuViewForTesting();
  }

  void ClickPinButton(ExtensionsMenuItemView* menu_item) const;

 private:
  base::AutoReset<bool> allow_extension_menu_instances_;
  base::test::ScopedFeatureList feature_list_;

  extensions::ExtensionService* extension_service_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsMenuViewUnitTest);
};

void ExtensionsMenuViewUnitTest::SetUp() {
  TestWithBrowserView::SetUp();

  // Set up some extension-y bits.
  extensions::TestExtensionSystem* extension_system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile()));
  extension_system->CreateExtensionService(
      base::CommandLine::ForCurrentProcess(), base::FilePath(), false);

  extension_service_ =
      extensions::ExtensionSystem::Get(profile())->extension_service();

  ExtensionsMenuView::ShowBubble(extensions_container()->extensions_button(),
                                 browser(), extensions_container());
}

scoped_refptr<const extensions::Extension>
ExtensionsMenuViewUnitTest::AddSimpleExtension(const char* name) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(name).Build();
  extension_service()->AddExtension(extension.get());

  return extension;
}

void ExtensionsMenuViewUnitTest::ClickPinButton(
    ExtensionsMenuItemView* menu_item) const {
  views::ImageButton* pin_button = menu_item->pin_button_for_testing();
  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, gfx::Point(1, 1),
                             gfx::Point(), ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  pin_button->OnMousePressed(press_event);
  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, gfx::Point(1, 1),
                               gfx::Point(), ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);
  pin_button->OnMouseReleased(release_event);
}

TEST_F(ExtensionsMenuViewUnitTest, ExtensionsAreShownInTheMenu) {
  // To start, there should be no extensions in the menu.
  EXPECT_EQ(0u, extensions_menu()->extensions_menu_items_for_testing().size());

  // Add an extension, and verify that it's added to the menu.
  constexpr char kExtensionName[] = "Test 1";
  AddSimpleExtension(kExtensionName);

  {
    std::vector<ExtensionsMenuItemView*> menu_items =
        extensions_menu()->extensions_menu_items_for_testing();
    ASSERT_EQ(1u, menu_items.size());
    EXPECT_EQ(kExtensionName,
              base::UTF16ToUTF8(menu_items[0]
                                    ->primary_action_button_for_testing()
                                    ->label_text_for_testing()));
  }
}

TEST_F(ExtensionsMenuViewUnitTest, PinnedExtensionAppearsInToolbar) {
  AddSimpleExtension("Test Name");

  ExtensionsMenuItemView* menu_item =
      extensions_menu()->extensions_menu_items_for_testing()[0];
  ToolbarActionViewController* controller =
      menu_item->view_controller_for_testing();
  EXPECT_FALSE(extensions_container()->IsActionVisibleOnToolbar(controller));

  ClickPinButton(menu_item);

  EXPECT_TRUE(extensions_container()->IsActionVisibleOnToolbar(controller));
}

// TODO(crbug.com/984654): Add tests for multiple extensions, reordering
// extensions, and showing in multiple windows.
