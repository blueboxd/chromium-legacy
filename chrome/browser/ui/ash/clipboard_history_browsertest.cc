// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <memory>

#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_history_controller.h"
#include "ash/shell.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"

namespace {

std::unique_ptr<views::Widget> CreateTestWidget() {
  auto widget = std::make_unique<views::Widget>();

  views::Widget::InitParams params;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  widget->Init(std::move(params));

  return widget;
}

void SetClipboardText(const std::string& text) {
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteText(base::ASCIIToUTF16(text));

  // ClipboardHistory will post a task to process clipboard data in order to
  // debounce multiple clipboard writes occurring in sequence. Here we give
  // ClipboardHistory the chance to run its posted tasks before proceeding.
  base::RunLoop().RunUntilIdle();
}

ash::ClipboardHistoryController* GetClipboardHistoryController() {
  return ash::Shell::Get()->clipboard_history_controller();
}

const std::list<ui::ClipboardData>& GetClipboardData() {
  return GetClipboardHistoryController()->history()->GetItems();
}

gfx::Rect GetClipboardHistoryMenuBoundsInScreen() {
  return GetClipboardHistoryController()->GetMenuBoundsInScreenForTest();
}

class ClipboardTestHelper {
 public:
  ClipboardTestHelper() = default;
  ~ClipboardTestHelper() = default;

  void Init() {
    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        ash::Shell::GetPrimaryRootWindow());
  }

  ui::test::EventGenerator* event_generator() { return event_generator_.get(); }

  void PressAndRelease(ui::KeyboardCode key, int modifiers) {
    event_generator_->PressKey(key, modifiers);
    event_generator_->ReleaseKey(key, modifiers);
  }

 private:
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
};

}  // namespace

// Verify clipboard history's features in the multiprofile environment.
class ClipboardHistoryWithMultiProfileBrowserTest
    : public chromeos::LoginManagerTest {
 public:
  ClipboardHistoryWithMultiProfileBrowserTest() : LoginManagerTest() {
    login_mixin_.AppendRegularUsers(2);
    account_id1_ = login_mixin_.users()[0].account_id;
    account_id2_ = login_mixin_.users()[1].account_id;

    feature_list_.InitAndEnableFeature(chromeos::features::kClipboardHistory);
  }

  ~ClipboardHistoryWithMultiProfileBrowserTest() override = default;

  ui::test::EventGenerator* GetEventGenerator() {
    return test_helper_->event_generator();
  }

 protected:
  void PressAndRelease(ui::KeyboardCode key, int modifiers = ui::EF_NONE) {
    test_helper_->PressAndRelease(key, modifiers);
  }

  void ShowContextMenuViaAccelerator() {
    PressAndRelease(ui::KeyboardCode::VKEY_V, ui::EF_COMMAND_DOWN);
  }

  void SetUpOnMainThread() override {
    chromeos::LoginManagerTest::SetUpOnMainThread();

    test_helper_ = std::make_unique<ClipboardTestHelper>();
    test_helper_->Init();
  }

  AccountId account_id1_;
  AccountId account_id2_;
  chromeos::LoginManagerMixin login_mixin_{&mixin_host_};

  std::unique_ptr<ClipboardTestHelper> test_helper_;

  base::test::ScopedFeatureList feature_list_;
};

// Verify that the clipboard data history belonging to different users does not
// interfere with each other.
IN_PROC_BROWSER_TEST_F(ClipboardHistoryWithMultiProfileBrowserTest,
                       DisconflictInMultiUser) {
  LoginUser(account_id1_);
  EXPECT_TRUE(GetClipboardData().empty());

  // Store text when the user1 is active.
  const std::string copypaste_data1("user1_text1");
  SetClipboardText(copypaste_data1);

  {
    const std::list<ui::ClipboardData>& data = GetClipboardData();
    EXPECT_EQ(1u, data.size());
    EXPECT_EQ(copypaste_data1, data.front().text());
  }

  // Log in as the user2. The clipboard history should be empty.
  chromeos::UserAddingScreen::Get()->Start();
  AddUser(account_id2_);
  EXPECT_TRUE(GetClipboardData().empty());

  // Store text when the user2 is active.
  const std::string copypaste_data2("user2_text1");
  SetClipboardText(copypaste_data2);

  {
    const std::list<ui::ClipboardData>& data = GetClipboardData();
    EXPECT_EQ(1u, data.size());
    EXPECT_EQ(copypaste_data2, data.front().text());
  }

  // Switch to the user1.
  user_manager::UserManager::Get()->SwitchActiveUser(account_id1_);

  // Store text when the user1 is active.
  const std::string copypaste_data3("user1_text2");
  SetClipboardText(copypaste_data3);

  {
    const std::list<ui::ClipboardData>& data = GetClipboardData();
    EXPECT_EQ(2u, data.size());

    // Note that items in |data| follow the time ordering. The most recent item
    // is always the first one.
    auto it = data.begin();
    EXPECT_EQ(copypaste_data3, it->text());

    std::advance(it, 1u);
    EXPECT_EQ(copypaste_data1, it->text());
  }
}

// Verifies that the history menu is anchored at the cursor's location when
// not having any textfield.
IN_PROC_BROWSER_TEST_F(ClipboardHistoryWithMultiProfileBrowserTest,
                       ShowHistoryMenuWhenNoTextfieldExists) {
  LoginUser(account_id1_);

  // Close the browser window to ensure that textfield does not exist.
  CloseAllBrowsers();

  // No clipboard data. So the clipboard history menu should not show.
  ASSERT_TRUE(GetClipboardData().empty());
  ShowContextMenuViaAccelerator();
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());

  SetClipboardText("test");

  const gfx::Point mouse_location =
      ash::Shell::Get()->GetPrimaryRootWindow()->bounds().CenterPoint();
  GetEventGenerator()->MoveMouseTo(mouse_location);
  ShowContextMenuViaAccelerator();

  // Verifies that the menu is anchored at the cursor's location.
  ASSERT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  const gfx::Point menu_origin =
      GetClipboardHistoryMenuBoundsInScreen().origin();
  EXPECT_EQ(mouse_location.x() +
                views::MenuConfig::instance().touchable_anchor_offset,
            menu_origin.x());
  EXPECT_EQ(mouse_location.y(), menu_origin.y());
}

IN_PROC_BROWSER_TEST_F(ClipboardHistoryWithMultiProfileBrowserTest,
                       ShouldPasteHistoryViaKeyboard) {
  LoginUser(account_id1_);
  CloseAllBrowsers();

  // Create a widget containing a single, focusable textfield.
  auto widget = CreateTestWidget();
  auto* textfield =
      widget->SetContentsView(std::make_unique<views::Textfield>());
  textfield->SetAccessibleName(base::UTF8ToUTF16("Textfield"));
  textfield->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

  // Show the widget.
  widget->SetBounds(gfx::Rect(0, 0, 100, 100));
  widget->Show();
  EXPECT_TRUE(widget->IsActive());

  // Focus the textfield and confirm initial state.
  textfield->RequestFocus();
  EXPECT_TRUE(textfield->HasFocus());
  EXPECT_TRUE(textfield->GetText().empty());

  // Write some things to the clipboard.
  SetClipboardText("A");
  SetClipboardText("B");
  SetClipboardText("C");

  // Verify we can paste the first history item via the ENTER key.
  PressAndRelease(ui::KeyboardCode::VKEY_V, ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_RETURN);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  EXPECT_EQ("C", base::UTF16ToUTF8(textfield->GetText()));

  textfield->SetText(base::string16());
  EXPECT_TRUE(textfield->GetText().empty());

  // Verify we can paste the first history item via the COMMAND+V shortcut.
  PressAndRelease(ui::KeyboardCode::VKEY_V, ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_V, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  EXPECT_EQ("C", base::UTF16ToUTF8(textfield->GetText()));

  textfield->SetText(base::string16());
  EXPECT_TRUE(textfield->GetText().empty());

  // Verify we can paste the last history item via the ENTER key.
  PressAndRelease(ui::KeyboardCode::VKEY_V, ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_RETURN);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  EXPECT_EQ("A", base::UTF16ToUTF8(textfield->GetText()));

  textfield->SetText(base::string16());
  EXPECT_TRUE(textfield->GetText().empty());

  // Verify we can paste the last history item via the COMMAND+V shortcut.
  PressAndRelease(ui::KeyboardCode::VKEY_V, ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_V, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  EXPECT_EQ("A", base::UTF16ToUTF8(textfield->GetText()));
}
