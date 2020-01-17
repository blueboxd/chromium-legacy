// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_handler.h"

#include <memory>

#include "base/macros.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_embedder.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_layout.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/default_theme_provider.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/geometry/point.h"

namespace {

class TestTabStripUIHandler : public TabStripUIHandler {
 public:
  explicit TestTabStripUIHandler(content::WebUI* web_ui,
                                 Browser* browser,
                                 TabStripUIEmbedder* embedder)
      : TabStripUIHandler(browser, embedder) {
    set_web_ui(web_ui);
  }
};

class MockTabStripUIEmbedder : public TabStripUIEmbedder {
 public:
  MockTabStripUIEmbedder() {}
  MOCK_CONST_METHOD0(GetAcceleratorProvider, const ui::AcceleratorProvider*());
  MOCK_METHOD0(CloseContainer, void());
  MOCK_METHOD2(ShowContextMenuAtPoint,
               void(gfx::Point, std::unique_ptr<ui::MenuModel>));
  MOCK_METHOD0(GetLayout, TabStripUILayout());
  MOCK_METHOD0(GetThemeProvider, const ui::ThemeProvider*());
};

}  // namespace

class TabStripUIHandlerTest : public BrowserWithTestWindowTest {
 public:
  TabStripUIHandlerTest() : web_ui_(new content::TestWebUI) {}
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    handler_ = std::make_unique<TestTabStripUIHandler>(web_ui(), browser(),
                                                       &mock_embedder_);
    handler()->AllowJavascriptForTesting();
    web_ui()->ClearTrackedCalls();
  }

  TabStripUIHandler* handler() { return handler_.get(); }
  content::TestWebUI* web_ui() { return web_ui_.get(); }

 protected:
  ::testing::NiceMock<MockTabStripUIEmbedder> mock_embedder_;

 private:
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<TestTabStripUIHandler> handler_;
  DISALLOW_COPY_AND_ASSIGN(TabStripUIHandlerTest);
};

TEST_F(TabStripUIHandlerTest, GroupClosedEvent) {
  AddTab(browser(), GURL("http://foo"));
  tab_groups::TabGroupId expected_group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});
  browser()->tab_strip_model()->RemoveFromGroup({0});

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data.function_name());

  std::string event_name;
  ASSERT_TRUE(data.arg1()->GetAsString(&event_name));
  EXPECT_EQ("tab-group-closed", event_name);

  std::string actual_group_id;
  ASSERT_TRUE(data.arg2()->GetAsString(&actual_group_id));
  EXPECT_EQ(expected_group_id.ToString(), actual_group_id);
}

TEST_F(TabStripUIHandlerTest, GroupStateChangedEvents) {
  AddTab(browser(), GURL("http://foo/1"));
  AddTab(browser(), GURL("http://foo/2"));

  // Add one of the tabs to a group to test for a tab-group-state-changed event.
  tab_groups::TabGroupId expected_group_id =
      browser()->tab_strip_model()->AddToNewGroup({0, 1});

  const content::TestWebUI::CallData& grouped_data =
      *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", grouped_data.function_name());

  std::string event_name;
  ASSERT_TRUE(grouped_data.arg1()->GetAsString(&event_name));
  EXPECT_EQ("tab-group-state-changed", event_name);

  int expected_tab_id = extensions::ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetWebContentsAt(1));
  int actual_tab_id;
  ASSERT_TRUE(grouped_data.arg2()->GetAsInteger(&actual_tab_id));
  EXPECT_EQ(expected_tab_id, actual_tab_id);

  int index;
  ASSERT_TRUE(grouped_data.arg3()->GetAsInteger(&index));
  EXPECT_EQ(1, index);

  std::string actual_group_id;
  ASSERT_TRUE(grouped_data.arg4()->GetAsString(&actual_group_id));
  EXPECT_EQ(expected_group_id.ToString(), actual_group_id);

  // Remove the tab from the group to test for a tab-group-state-changed event.
  browser()->tab_strip_model()->RemoveFromGroup({1});
  const content::TestWebUI::CallData& ungrouped_data =
      *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", ungrouped_data.function_name());
  ASSERT_TRUE(ungrouped_data.arg1()->GetAsString(&event_name));
  EXPECT_EQ("tab-group-state-changed", event_name);
  ASSERT_TRUE(ungrouped_data.arg2()->GetAsInteger(&actual_tab_id));
  EXPECT_EQ(expected_tab_id, actual_tab_id);
  ASSERT_TRUE(ungrouped_data.arg3()->GetAsInteger(&index));
  EXPECT_EQ(1, index);
  EXPECT_EQ(nullptr, ungrouped_data.arg4());
}

TEST_F(TabStripUIHandlerTest, GroupMovedEvents) {
  // Create a tab group and a few other tabs to allow the group to move.
  AddTab(browser(), GURL("http://foo/1"));
  AddTab(browser(), GURL("http://foo/2"));
  AddTab(browser(), GURL("http://foo/3"));
  AddTab(browser(), GURL("http://foo/4"));
  tab_groups::TabGroupId expected_group_id =
      browser()->tab_strip_model()->AddToNewGroup({0, 1});

  // Select all the tabs in the group.
  ui::ListSelectionModel selection;
  selection.AddIndexToSelection(0);
  selection.AddIndexToSelection(1);
  selection.set_active(0);
  browser()->tab_strip_model()->SetSelectionFromModel(selection);

  web_ui()->ClearTrackedCalls();

  // Move the selected tabs to later in the tab strip. This should result in
  // a single event that is fired to indicate the entire group has moved.
  int expected_index = 2;
  browser()->tab_strip_model()->MoveSelectedTabsTo(expected_index);

  EXPECT_EQ(1U, web_ui()->call_data().size());

  const content::TestWebUI::CallData& grouped_data =
      *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", grouped_data.function_name());

  std::string event_name;
  ASSERT_TRUE(grouped_data.arg1()->GetAsString(&event_name));
  EXPECT_EQ("tab-group-moved", event_name);

  std::string actual_group_id;
  ASSERT_TRUE(grouped_data.arg2()->GetAsString(&actual_group_id));
  EXPECT_EQ(expected_group_id.ToString(), actual_group_id);

  int actual_index;
  ASSERT_TRUE(grouped_data.arg3()->GetAsInteger(&actual_index));
  EXPECT_EQ(expected_index, actual_index);

  web_ui()->ClearTrackedCalls();

  // Move the selected tabs to earlier in the tab strip. This should also
  // result in a single event that is fired to indicate the entire group has
  // moved.
  expected_index = 1;
  browser()->tab_strip_model()->MoveSelectedTabsTo(expected_index);

  EXPECT_EQ(1U, web_ui()->call_data().size());

  const content::TestWebUI::CallData& grouped_data2 =
      *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", grouped_data2.function_name());
  ASSERT_TRUE(grouped_data2.arg1()->GetAsString(&event_name));
  EXPECT_EQ("tab-group-moved", event_name);
  ASSERT_TRUE(grouped_data2.arg2()->GetAsString(&actual_group_id));
  EXPECT_EQ(expected_group_id.ToString(), actual_group_id);
  ASSERT_TRUE(grouped_data2.arg3()->GetAsInteger(&actual_index));
  EXPECT_EQ(expected_index, actual_index);
}
