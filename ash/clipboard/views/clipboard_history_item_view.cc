// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_item_view.h"

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/views/clipboard_history_bitmap_item_view.h"
#include "ash/clipboard/views/clipboard_history_text_item_view.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/views/border.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/layout/fill_layout.h"

namespace {

// The insets within the contents view.
constexpr gfx::Insets kContentsInsets(/*vertical=*/4, /*horizontal=*/16);

// The view responding to mouse click or gesture tap events.
class MainButton : public views::Button {
 public:
  MainButton(views::ButtonListener* listener) : Button(listener) {}
  MainButton(const MainButton& rhs) = delete;
  MainButton& operator=(const MainButton& rhs) = delete;
  ~MainButton() override = default;
};
}  // namespace

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// ClipboardHistoryItemView::ContentsView

ClipboardHistoryItemView::ContentsView::ContentsView() {
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
  SetBorder(views::CreateEmptyBorder(kContentsInsets));
}

ClipboardHistoryItemView::ContentsView::~ContentsView() = default;

// The contents view's default behavior is to reject any event. It gives the
// main button of the menu item a chance to handle events.
bool ClipboardHistoryItemView::ContentsView::DoesIntersectRect(
    const views::View* target,
    const gfx::Rect& rect) const {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// ClipboardHistoryItemView

// static
std::unique_ptr<ClipboardHistoryItemView>
ClipboardHistoryItemView::CreateFromClipboardHistoryItem(
    const ClipboardHistoryItem& item,
    views::MenuItemView* container) {
  switch (ClipboardHistoryUtil::CalculateMainFormat(item.data()).value()) {
    case ui::ClipboardInternalFormat::kBitmap:
      return std::make_unique<ClipboardHistoryBitmapItemView>(item, container);
    case ui::ClipboardInternalFormat::kText:
    case ui::ClipboardInternalFormat::kHtml:
    case ui::ClipboardInternalFormat::kRtf:
    case ui::ClipboardInternalFormat::kBookmark:
    case ui::ClipboardInternalFormat::kWeb:
    case ui::ClipboardInternalFormat::kCustom:
      return std::make_unique<ClipboardHistoryTextItemView>(item, container);
  }
}

ClipboardHistoryItemView::~ClipboardHistoryItemView() = default;

ClipboardHistoryItemView::ClipboardHistoryItemView(
    views::MenuItemView* container)
    : container_(container) {}

void ClipboardHistoryItemView::Init() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  AddChildView(std::make_unique<MainButton>(this));

  auto contents_view = CreateContentsView();
  AddChildView(std::move(contents_view));
}

gfx::Size ClipboardHistoryItemView::CalculatePreferredSize() const {
  const int preferred_width =
      views::MenuConfig::instance().touchable_menu_width;
  return gfx::Size(preferred_width, GetHeightForWidth(preferred_width));
}

void ClipboardHistoryItemView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  container_->GetDelegate()->ExecuteCommand(container_->GetCommand(),
                                            event.flags());
}

}  // namespace ash
