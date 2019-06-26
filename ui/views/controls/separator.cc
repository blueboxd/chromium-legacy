// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/separator.h"

#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/canvas.h"
#include "ui/native_theme/native_theme.h"

namespace views {

// static
const int Separator::kThickness = 1;

Separator::Separator() = default;

Separator::~Separator() = default;

SkColor Separator::GetColor() const {
  if (overridden_color_ == true)
    return overridden_color_.value();
  return 0;
}

void Separator::SetColor(SkColor color) {
  if (overridden_color_ == color)
    return;

  overridden_color_ = color;
  OnPropertyChanged(&overridden_color_, kPropertyEffectsPaint);
}

int Separator::GetPreferredHeight() const {
  return preferred_height_;
}

void Separator::SetPreferredHeight(int height) {
  if (preferred_height_ == height)
    return;

  preferred_height_ = height;
  OnPropertyChanged(&preferred_height_, kPropertyEffectsPreferredSizeChanged);
}

////////////////////////////////////////////////////////////////////////////////
// Separator, View overrides:

gfx::Size Separator::CalculatePreferredSize() const {
  gfx::Size size(kThickness, preferred_height_);
  gfx::Insets insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

void Separator::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kSplitter;
}

void Separator::OnPaint(gfx::Canvas* canvas) {
  SkColor color = overridden_color_
                      ? *overridden_color_
                      : GetNativeTheme()->GetSystemColor(
                            ui::NativeTheme::kColorId_SeparatorColor);

  float dsf = canvas->UndoDeviceScaleFactor();

  // The separator fills its bounds, but avoid filling partial pixels.
  gfx::Rect aligned = gfx::ScaleToEnclosedRect(GetContentsBounds(), dsf, dsf);

  // At least 1 pixel should be drawn to make the separator visible.
  aligned.set_width(std::max(1, aligned.width()));
  aligned.set_height(std::max(1, aligned.height()));
  canvas->FillRect(aligned, color);

  View::OnPaint(canvas);
}

BEGIN_METADATA(Separator)
METADATA_PARENT_CLASS(View)
ADD_PROPERTY_METADATA(Separator, SkColor, Color)
ADD_PROPERTY_METADATA(Separator, int, PreferredHeight)
END_METADATA()

}  // namespace views
