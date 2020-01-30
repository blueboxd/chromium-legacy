// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_BASE_ASSISTANT_SCROLL_VIEW_H_
#define ASH_ASSISTANT_UI_BASE_ASSISTANT_SCROLL_VIEW_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view_observer.h"

namespace ash {

class COMPONENT_EXPORT(ASSISTANT_UI) AssistantScrollView
    : public views::ScrollView,
      views::ViewObserver {
 public:
  AssistantScrollView();
  ~AssistantScrollView() override;

  // views::ScrollView:
  const char* GetClassName() const override;

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* view) override;

  virtual void OnContentsPreferredSizeChanged(views::View* content_view) = 0;

  virtual void OnScrollBarUpdated(views::ScrollBar* scroll_bar,
                                  int viewport_size,
                                  int content_size,
                                  int content_scroll_offset) {}

  virtual void OnScrollBarVisibilityChanged(views::ScrollBar* scroll_bar,
                                            bool is_visible) {}

 protected:
  views::View* content_view() { return content_view_; }
  const views::View* content_view() const { return content_view_; }

  views::ScrollBar* horizontal_scroll_bar() { return horizontal_scroll_bar_; }
  views::ScrollBar* vertical_scroll_bar() { return vertical_scroll_bar_; }

 private:
  void InitLayout();

  views::View* content_view_;                // Owned by view hierarchy.
  views::ScrollBar* horizontal_scroll_bar_;  // Owned by view hierarchy.
  views::ScrollBar* vertical_scroll_bar_;    // Owned by view hierarchy.

  DISALLOW_COPY_AND_ASSIGN(AssistantScrollView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_BASE_ASSISTANT_SCROLL_VIEW_H_
