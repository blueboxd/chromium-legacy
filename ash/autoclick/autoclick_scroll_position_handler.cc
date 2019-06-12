// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/autoclick/autoclick_scroll_position_handler.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/transform.h"
#include "ui/views/view.h"

namespace ash {

namespace {
int kScrollBackgroundSizeInDips = 32;
int kScrollIconSizeInDips = 24;
SkColor kIconBackgroundColor = SkColorSetARGB(255, 128, 134, 139);
}  // namespace

// View of the AutoclickScrollPositionHandler. Draws the actual contents and
// maintains the views::Widget that the animation is shown in.
class AutoclickScrollPositionView : public views::View {
 public:
  AutoclickScrollPositionView(const gfx::Point& event_location,
                              views::Widget* widget)
      : views::View(), widget_(widget) {
    image_ = gfx::CreateVectorIcon(kAutoclickScrollIcon, kScrollIconSizeInDips,
                                   SK_ColorWHITE);
    widget_->SetContentsView(this);
    SetSize(
        gfx::Size(kScrollBackgroundSizeInDips, kScrollBackgroundSizeInDips));

    // Owned by the AutoclickScrollPositionHandler.
    set_owned_by_client();

    SetLocation(event_location);
  }

  ~AutoclickScrollPositionView() override = default;

  void SetLocation(const gfx::Point& new_event_location) {
    gfx::Point point = new_event_location;
    widget_->SetBounds(gfx::Rect(point.x() - kScrollBackgroundSizeInDips / 2,
                                 point.y() - kScrollBackgroundSizeInDips / 2,
                                 kScrollBackgroundSizeInDips,
                                 kScrollBackgroundSizeInDips));
    widget_->Show();
    SchedulePaint();
  }

 private:
  void OnPaint(gfx::Canvas* canvas) override {
    gfx::Point center(kScrollBackgroundSizeInDips / 2,
                      kScrollBackgroundSizeInDips / 2);
    canvas->Save();

    cc::PaintFlags flags;
    flags.setAntiAlias(true);

    // Draw the grey background.
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(kIconBackgroundColor);
    canvas->DrawCircle(center, kScrollBackgroundSizeInDips / 2, flags);

    // Draw the icon on top.
    canvas->DrawImageInt(image_, center.x() - kScrollIconSizeInDips / 2,
                         center.y() - kScrollIconSizeInDips / 2);

    canvas->Restore();
  }

  views::Widget* widget_;
  gfx::ImageSkia image_;

  DISALLOW_COPY_AND_ASSIGN(AutoclickScrollPositionView);
};

AutoclickScrollPositionHandler::AutoclickScrollPositionHandler(
    const gfx::Point& center_point_in_screen,
    views::Widget* widget) {
  view_ = std::make_unique<AutoclickScrollPositionView>(center_point_in_screen,
                                                        widget);
}

AutoclickScrollPositionHandler::~AutoclickScrollPositionHandler() {
  view_.reset();
}

void AutoclickScrollPositionHandler::SetCenter(
    const gfx::Point& center_point_in_screen,
    views::Widget* widget) {
  view_->SetLocation(center_point_in_screen);
}

}  // namespace ash
