// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/tab_drag_drop_delegate.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/ash_features.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/pickle.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/vector2d.h"

using ::testing::_;
using ::testing::Return;

namespace ash {

namespace {

std::unique_ptr<ui::OSExchangeData> MakeDragData(const std::string& mime_type,
                                                 const std::string& data) {
  auto result = std::make_unique<ui::OSExchangeData>();

  base::flat_map<base::string16, base::string16> data_map;
  data_map.emplace(base::ASCIIToUTF16(mime_type), base::ASCIIToUTF16(data));

  base::Pickle inner_data;
  ui::WriteCustomDataToPickle(data_map, &inner_data);

  result->SetPickledData(ui::ClipboardFormatType::GetWebCustomDataType(),
                         inner_data);
  return result;
}

class MockShellDelegate : public TestShellDelegate {
 public:
  MockShellDelegate() = default;
  ~MockShellDelegate() override = default;

  MOCK_METHOD(aura::Window*,
              CreateBrowserForTabDrop,
              (aura::Window*, const ui::OSExchangeData&),
              (override));
};

static constexpr char kTabMimeType[] = "application/vnd.chromium.tab";

}  // namespace

class TabDragDropDelegateTest : public AshTestBase {
 public:
  TabDragDropDelegateTest() {
    ash::features::SetWebUITabStripEnabled(true);
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kWebUITabStripTabDragIntegration);
  }

  // AshTestBase:
  void SetUp() override {
    auto mock_shell_delegate = std::make_unique<MockShellDelegate>();
    mock_shell_delegate_ = mock_shell_delegate.get();
    AshTestBase::SetUp(std::move(mock_shell_delegate));
    ash::TabletModeControllerTestApi().EnterTabletMode();
  }

  void TearDown() override {
    // Clear our pointer before the object is destroyed.
    mock_shell_delegate_ = nullptr;
    AshTestBase::TearDown();
  }

  MockShellDelegate* mock_shell_delegate() { return mock_shell_delegate_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  MockShellDelegate* mock_shell_delegate_ = nullptr;
};

TEST_F(TabDragDropDelegateTest, AcceptsValidDrags) {
  EXPECT_TRUE(
      TabDragDropDelegate::IsChromeTabDrag(*MakeDragData(kTabMimeType, "foo")));
}

TEST_F(TabDragDropDelegateTest, RejectsInvalidDrags) {
  EXPECT_FALSE(
      TabDragDropDelegate::IsChromeTabDrag(*MakeDragData("text/plain", "bar")));
}

TEST_F(TabDragDropDelegateTest, DragToExistingTabStrip) {
  // Create a fake source window. Its details don't matter.
  aura::Window* const source_window =
      CreateTestWindowInShellWithBounds(gfx::Rect(0, 0, 1, 1));

  // A new window shouldn't be created in this case.
  EXPECT_CALL(*mock_shell_delegate(), CreateBrowserForTabDrop(_, _)).Times(0);

  // Emulate a drag session whose drop target accepts the drop. In this
  // case, TabDragDropDelegate::Drop() is not called.
  TabDragDropDelegate delegate(Shell::GetPrimaryRootWindow(), source_window,
                               gfx::Point(0, 0));
  delegate.DragUpdate(gfx::Point(1, 0));
  delegate.DragUpdate(gfx::Point(2, 0));

  // Let |delegate| be destroyed without a Drop() call.
}

TEST_F(TabDragDropDelegateTest, DragToNewWindow) {
  // Create the source window. This should automatically fill the work area
  // since we're in tablet mode.
  std::unique_ptr<aura::Window> source_window = CreateToplevelTestWindow();

  EXPECT_FALSE(
      SplitViewController::Get(source_window.get())->InTabletSplitViewMode());

  const gfx::Point drag_start_location = source_window->bounds().CenterPoint();
  // Emulate a drag session ending in a drop to a new window.
  TabDragDropDelegate delegate(Shell::GetPrimaryRootWindow(),
                               source_window.get(), drag_start_location);
  delegate.DragUpdate(drag_start_location);
  delegate.DragUpdate(drag_start_location + gfx::Vector2d(1, 0));
  delegate.DragUpdate(drag_start_location + gfx::Vector2d(2, 0));

  // Check that a new window is requested. Assume the correct drop data
  // is passed. Return the new window.
  std::unique_ptr<aura::Window> new_window = CreateToplevelTestWindow();
  EXPECT_CALL(*mock_shell_delegate(),
              CreateBrowserForTabDrop(source_window.get(), _))
      .Times(1)
      .WillOnce(Return(new_window.get()));

  auto drop_data = MakeDragData(kTabMimeType, "fake_id");
  delegate.Drop(drag_start_location + gfx::Vector2d(2, 0), *drop_data);

  EXPECT_FALSE(
      SplitViewController::Get(source_window.get())->InTabletSplitViewMode());
}

TEST_F(TabDragDropDelegateTest, DropOnEdgeEntersSplitView) {
  // Create the source window. This should automatically fill the work area
  // since we're in tablet mode.
  std::unique_ptr<aura::Window> source_window = CreateToplevelTestWindow();

  // Emulate a drag to the right edge of the screen.
  const gfx::Point drag_start_location = source_window->bounds().CenterPoint();
  const gfx::Point drag_end_location =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          source_window.get())
          .right_center();

  TabDragDropDelegate delegate(Shell::GetPrimaryRootWindow(),
                               source_window.get(), drag_start_location);
  delegate.DragUpdate(drag_start_location);
  delegate.DragUpdate(drag_end_location);

  std::unique_ptr<aura::Window> new_window = CreateToplevelTestWindow();
  EXPECT_CALL(*mock_shell_delegate(),
              CreateBrowserForTabDrop(source_window.get(), _))
      .Times(1)
      .WillOnce(Return(new_window.get()));

  auto drop_data = MakeDragData(kTabMimeType, "fake_id");
  delegate.Drop(drag_end_location, *drop_data);

  SplitViewController* const split_view_controller =
      SplitViewController::Get(source_window.get());
  EXPECT_TRUE(split_view_controller->InTabletSplitViewMode());
  EXPECT_EQ(new_window.get(), split_view_controller->GetSnappedWindow(
                                  SplitViewController::SnapPosition::RIGHT));
}

}  // namespace ash
