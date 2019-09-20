// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_HEADER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_HEADER_H_

#include "chrome/browser/ui/tabs/tab_group_id.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "ui/views/widget/widget_observer.h"

class TabController;
class TabGroupVisualData;
struct TabSizeInfo;

namespace views {
class Label;
class View;
}

// View for tab group headers in the tab strip, which are markers of group
// boundaries. There is one header for each group, which is included in the tab
// strip flow and positioned left of the leftmost tab in the group.
class TabGroupHeader : public TabSlotView {
 public:
  TabGroupHeader(TabController* controller, TabGroupId group);

  // TabSlotView:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  TabSizeInfo GetTabSizeInfo() const override;

  TabGroupId group() const { return group_; }

  // Updates our visual state according to the TabGroupVisualData for our group.
  void VisualsChanged();

 private:
  // Calculate the width for this View.
  int CalculateWidth() const;

  TabController* const controller_;
  const TabGroupId group_;

  views::View* title_chip_;
  views::Label* title_;

  // Tracks whether our editor bubble is open. At most one can be open
  // at once.
  class EditorBubbleTracker : public views::WidgetObserver {
   public:
    EditorBubbleTracker() = default;
    ~EditorBubbleTracker() override = default;

    void Opened(views::Widget* bubble_widget);
    bool is_open() const { return is_open_; }

    // views::WidgetObserver:
    void OnWidgetDestroyed(views::Widget* widget) override;

   private:
    bool is_open_ = false;
  };

  EditorBubbleTracker editor_bubble_tracker_;

  DISALLOW_COPY_AND_ASSIGN(TabGroupHeader);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_HEADER_H_
