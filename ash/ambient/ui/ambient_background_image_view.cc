// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_background_image_view.h"

#include "ash/assistant/ui/assistant_view_ids.h"
#include "ui/events/event.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace ash {

AmbientBackgroundImageView::AmbientBackgroundImageView(
    AmbientViewDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
  SetID(AssistantViewID::kAmbientBackgroundImageView);
}

AmbientBackgroundImageView::~AmbientBackgroundImageView() = default;

// views::View:
bool AmbientBackgroundImageView::OnMousePressed(const ui::MouseEvent& event) {
  delegate_->OnBackgroundPhotoEvents();
  return true;
}

// views::View:
void AmbientBackgroundImageView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    delegate_->OnBackgroundPhotoEvents();
    event->SetHandled();
  }
}

BEGIN_METADATA(AmbientBackgroundImageView)
METADATA_PARENT_CLASS(views::ImageView)
END_METADATA()

}  // namespace ash
