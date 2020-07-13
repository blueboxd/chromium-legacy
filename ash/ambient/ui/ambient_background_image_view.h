// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_BACKGROUND_IMAGE_VIEW_H_
#define ASH_AMBIENT_UI_AMBIENT_BACKGROUND_IMAGE_VIEW_H_

#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ash_export.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

// AmbientBackgroundImageView--------------------------------------------------
// A custom ImageView for ambient mode to handle specific mouse/gesture events
// when the user interacts with the background photos.
class ASH_EXPORT AmbientBackgroundImageView : public views::ImageView {
 public:
  METADATA_HEADER(AmbientBackgroundImageView);

  explicit AmbientBackgroundImageView(AmbientViewDelegate* delegate);
  AmbientBackgroundImageView(const AmbientBackgroundImageView&) = delete;
  AmbientBackgroundImageView& operator=(AmbientBackgroundImageView&) = delete;
  ~AmbientBackgroundImageView() override;

  // views::View
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  // Owned by |AmbientController| and should always outlive |this|.
  AmbientViewDelegate* delegate_ = nullptr;
};
}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_BACKGROUND_IMAGE_VIEW_H_
