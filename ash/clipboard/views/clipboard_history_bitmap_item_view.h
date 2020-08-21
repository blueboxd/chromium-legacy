// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_BITMAP_ITEM_VIEW_H_
#define ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_BITMAP_ITEM_VIEW_H_

#include "ash/clipboard/views/clipboard_history_item_view.h"

class SkBitmap;

namespace views {
class ImageView;
}  // namespace views

namespace ash {

// The menu item showing the bitmap.
class ClipboardHistoryBitmapItemView : public ClipboardHistoryItemView {
 public:
  ClipboardHistoryBitmapItemView(const ClipboardHistoryItem& item,
                                 views::MenuItemView* container);
  ClipboardHistoryBitmapItemView(const ClipboardHistoryBitmapItemView& rhs) =
      delete;
  ClipboardHistoryBitmapItemView& operator=(
      const ClipboardHistoryBitmapItemView& rhs) = delete;
  ~ClipboardHistoryBitmapItemView() override;

 private:
  // ClipboardHistoryItemView:
  const char* GetClassName() const override;
  std::unique_ptr<ContentsView> CreateContentsView() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // Calculates the target size of the image to show.
  gfx::Size CalculateTargetImageSize() const;

  // Bitmap stored in the clipboard data.
  const SkBitmap* const original_bitmap_;

  // Owned by view hierarchy.
  views::ImageView* image_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_BITMAP_ITEM_VIEW_H_
