// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_text_item_view.h"

#include "ash/clipboard/clipboard_history_controller.h"
#include "ash/clipboard/clipboard_history_resource_manager.h"
#include "ash/shell.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/layout/fill_layout.h"

namespace {

// The preferred height for the label.
constexpr int kLabelPreferredHeight = 16;
}  // namespace

namespace ash {

ClipboardHistoryTextItemView::ClipboardHistoryTextItemView(
    const ClipboardHistoryItem& item,
    views::MenuItemView* container)
    : ClipboardHistoryItemView(container),
      text_(Shell::Get()
                ->clipboard_history_controller()
                ->resource_manager()
                ->GetLabel(item)) {}

ClipboardHistoryTextItemView::~ClipboardHistoryTextItemView() = default;

const char* ClipboardHistoryTextItemView::GetClassName() const {
  return "ClipboardHistoryTextItemView";
}

std::unique_ptr<ClipboardHistoryTextItemView::ContentsView>
ClipboardHistoryTextItemView::CreateContentsView() {
  auto contents_view = std::make_unique<ContentsView>();
  contents_view->SetLayoutManager(std::make_unique<views::FillLayout>());

  auto label = std::make_unique<views::Label>(text_);
  label->SetPreferredSize(gfx::Size(INT_MAX, kLabelPreferredHeight));
  label->SetFontList(views::MenuConfig::instance().font_list);
  label->SetMultiLine(false);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  contents_view->AddChildView(std::move(label));

  return contents_view;
}

}  // namespace ash
