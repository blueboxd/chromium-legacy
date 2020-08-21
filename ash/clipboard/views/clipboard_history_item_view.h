// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_ITEM_VIEW_H_
#define ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_ITEM_VIEW_H_

#include "ui/views/controls/button/button.h"
#include "ui/views/view_targeter_delegate.h"

namespace views {
class MenuItemView;
}  // namespace views

namespace ash {
class ClipboardHistoryItem;

// The base class for menu items of the clipboard history menu.
class ClipboardHistoryItemView : public views::View,
                                 public views::ButtonListener {
 public:
  static std::unique_ptr<ClipboardHistoryItemView>
  CreateFromClipboardHistoryItem(const ClipboardHistoryItem& item,
                                 views::MenuItemView* container);

  ClipboardHistoryItemView(const ClipboardHistoryItemView& rhs) = delete;
  ClipboardHistoryItemView& operator=(const ClipboardHistoryItemView& rhs) =
      delete;
  ~ClipboardHistoryItemView() override;

  // Initializes the menu item.
  void Init();

 protected:
  // Used by subclasses to draw contents, such as text or bitmaps.
  class ContentsView : public views::View, public views::ViewTargeterDelegate {
   public:
    ContentsView();
    ContentsView(const ContentsView& rhs) = delete;
    ContentsView& operator=(const ContentsView& rhs) = delete;
    ~ContentsView() override;

    // views::ViewTargeterDelegate:
    bool DoesIntersectRect(const views::View* target,
                           const gfx::Rect& rect) const override;
  };

  explicit ClipboardHistoryItemView(views::MenuItemView* container);

  // Creates the contents view.
  virtual std::unique_ptr<ContentsView> CreateContentsView() = 0;

 private:
  // views::View:
  gfx::Size CalculatePreferredSize() const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  views::MenuItemView* const container_;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_ITEM_VIEW_H_
