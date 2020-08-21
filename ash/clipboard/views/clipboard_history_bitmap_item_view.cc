// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_bitmap_item_view.h"

#include "ash/clipboard/clipboard_history_item.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"

namespace {

// The preferred height for the bitmap.
constexpr int kBitmapHeight = 64;

// The contents area's rounded corners.
constexpr gfx::RoundedCornersF kRoundedCorners = gfx::RoundedCornersF(4);
}  // namespace

namespace ash {

ClipboardHistoryBitmapItemView::ClipboardHistoryBitmapItemView(
    const ClipboardHistoryItem& item,
    views::MenuItemView* container)
    : ClipboardHistoryItemView(container),
      original_bitmap_(&(item.data().bitmap())) {}

ClipboardHistoryBitmapItemView::~ClipboardHistoryBitmapItemView() = default;

const char* ClipboardHistoryBitmapItemView::GetClassName() const {
  return "ClipboardHistoryBitmapItemView";
}

std::unique_ptr<ClipboardHistoryBitmapItemView::ContentsView>
ClipboardHistoryBitmapItemView::CreateContentsView() {
  auto contents_view = std::make_unique<ContentsView>();
  contents_view->SetLayoutManager(std::make_unique<views::FillLayout>());

  auto image_view = std::make_unique<views::ImageView>();
  const gfx::ImageSkia img_from_bitmap =
      gfx::ImageSkia::CreateFrom1xBitmap(*original_bitmap_);
  image_view->SetImage(img_from_bitmap);
  image_view->SetPreferredSize(gfx::Size(INT_MAX, kBitmapHeight));
  image_view->SetPaintToLayer();
  image_view->layer()->SetRoundedCornerRadius(kRoundedCorners);
  image_view_ = contents_view->AddChildView(std::move(image_view));

  return contents_view;
}

void ClipboardHistoryBitmapItemView::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  image_view_->SetImageSize(CalculateTargetImageSize());
}

gfx::Size ClipboardHistoryBitmapItemView::CalculateTargetImageSize() const {
  const gfx::Size image_size(original_bitmap_->width(),
                             original_bitmap_->height());
  const double width_ratio = image_size.width() / double(width());
  const double height_ratio = image_size.height() / double(height());

  if (width_ratio <= 1.f || height_ratio <= 1.f)
    return image_size;

  const double resize_ratio = std::fmin(width_ratio, height_ratio);
  return gfx::Size(image_size.width() / resize_ratio,
                   image_size.height() / resize_ratio);
}

}  // namespace ash
