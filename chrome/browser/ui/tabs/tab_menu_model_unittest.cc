// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_menu_model.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/menu_model_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/menu_model.h"

class TabMenuModelTest : public MenuModelTest,
                         public BrowserWithTestWindowTest {
};

TEST_F(TabMenuModelTest, Basics) {
  chrome::NewTab(browser());
  TabMenuModel model(&delegate_, browser()->tab_strip_model(), 0);

  // Verify it has items. The number varies by platform, so we don't check
  // the exact number.
  EXPECT_GT(model.GetItemCount(), 5);

  int item_count = 0;
  CountEnabledExecutable(&model, &item_count);
  EXPECT_GT(item_count, 0);
  EXPECT_EQ(item_count, delegate_.execute_count_);
  EXPECT_EQ(item_count, delegate_.enable_count_);
}

TEST_F(TabMenuModelTest, MoveToNewWindow) {
  chrome::NewTab(browser());
  TabMenuModel model(&delegate_, browser()->tab_strip_model(), 0);

  // Verify that CommandMoveTabsToNewWindow is in the menu.
  EXPECT_GT(
      model.GetIndexOfCommandId(TabStripModel::CommandMoveTabsToNewWindow), -1);
}

TEST_F(TabMenuModelTest, AddToExistingGroupSubmenu) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kTabGroups);

  chrome::NewTab(browser());
  chrome::NewTab(browser());
  chrome::NewTab(browser());
  chrome::NewTab(browser());

  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  tab_strip_model->AddToNewGroup({0});
  tab_strip_model->AddToNewGroup({1});
  tab_strip_model->AddToNewGroup({2});

  TabMenuModel menu(&delegate_, tab_strip_model, 3);

  int submenu_index =
      menu.GetIndexOfCommandId(TabStripModel::CommandAddToExistingGroup);
  ui::MenuModel* submenu = menu.GetSubmenuModelAt(submenu_index);

  EXPECT_EQ(submenu->GetItemCount(), 5);
  EXPECT_TRUE(submenu->HasIcons());
}
