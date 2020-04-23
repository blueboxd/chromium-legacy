// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/themed_vector_icon.h"

#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"

namespace ui {

ThemedVectorIcon::ThemedVectorIcon() = default;

ThemedVectorIcon::ThemedVectorIcon(const gfx::VectorIcon* icon,
                                   NativeTheme::ColorId color_id,
                                   int icon_size)
    : icon_(icon), icon_size_(icon_size), color_id_(color_id) {}

ThemedVectorIcon::ThemedVectorIcon(const VectorIconModel& vector_icon_model)
    : icon_(vector_icon_model.vector_icon()),
      icon_size_(vector_icon_model.icon_size()) {
  if (vector_icon_model.has_color()) {
    color_ = vector_icon_model.color();
  } else if (vector_icon_model.color_id() >= 0) {
    color_id_ =
        static_cast<ui::NativeTheme::ColorId>(vector_icon_model.color_id());
  } else {
    color_id_ = ui::NativeTheme::kColorId_MenuIconColor;
  }
}

ThemedVectorIcon::ThemedVectorIcon(const gfx::VectorIcon* icon,
                                   SkColor color,
                                   int icon_size)
    : icon_(icon), icon_size_(icon_size), color_(color) {}

ThemedVectorIcon::ThemedVectorIcon(const ThemedVectorIcon&) = default;

ThemedVectorIcon& ThemedVectorIcon::operator=(const ThemedVectorIcon&) =
    default;

ThemedVectorIcon::ThemedVectorIcon(ThemedVectorIcon&&) = default;

ThemedVectorIcon& ThemedVectorIcon::operator=(ThemedVectorIcon&&) = default;

const gfx::ImageSkia ThemedVectorIcon::GetImageSkia(NativeTheme* theme) const {
  DCHECK(!empty());
  return icon_size_ > 0 ? CreateVectorIcon(*icon_, icon_size_, GetColor(theme))
                        : CreateVectorIcon(*icon_, GetColor(theme));
}

const gfx::ImageSkia ThemedVectorIcon::GetImageSkia(NativeTheme* theme,
                                                    int icon_size) const {
  DCHECK(!empty());
  return CreateVectorIcon(*icon_, icon_size, GetColor(theme));
}

const gfx::ImageSkia ThemedVectorIcon::GetImageSkia(SkColor color) const {
  DCHECK(!empty());
  return icon_size_ > 0 ? CreateVectorIcon(*icon_, icon_size_, color)
                        : CreateVectorIcon(*icon_, color);
}

SkColor ThemedVectorIcon::GetColor(NativeTheme* theme) const {
  DCHECK(color_id_ || color_);
  return color_id_ ? theme->GetSystemColor(color_id_.value()) : color_.value();
}

}  // namespace ui
