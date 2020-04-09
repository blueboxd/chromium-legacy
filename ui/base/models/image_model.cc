// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/image_model.h"

namespace ui {

VectorIconModel::VectorIconModel() = default;

VectorIconModel::VectorIconModel(const gfx::VectorIcon& vector_icon,
                                 int color_id,
                                 int icon_size)
    : vector_icon_(&vector_icon), icon_size_(icon_size), color_id_(color_id) {}

VectorIconModel::VectorIconModel(const gfx::VectorIcon& vector_icon,
                                 SkColor color,
                                 int icon_size)
    : vector_icon_(&vector_icon), icon_size_(icon_size), color_(color) {}

VectorIconModel::~VectorIconModel() = default;

VectorIconModel::VectorIconModel(const VectorIconModel&) = default;

VectorIconModel& VectorIconModel::operator=(const VectorIconModel&) = default;

VectorIconModel::VectorIconModel(VectorIconModel&&) = default;

VectorIconModel& VectorIconModel::operator=(VectorIconModel&&) = default;

ImageModel::ImageModel() = default;

ImageModel::ImageModel(const VectorIconModel& vector_icon_model)
    : vector_icon_model_(vector_icon_model) {}

ImageModel::ImageModel(const gfx::Image& image) : image_(image) {}

ImageModel::ImageModel(const gfx::ImageSkia& image_skia)
    : ImageModel(gfx::Image(image_skia)) {}

ImageModel::~ImageModel() = default;

ImageModel::ImageModel(const ImageModel&) = default;

ImageModel& ImageModel::operator=(const ImageModel&) = default;

ImageModel::ImageModel(ImageModel&&) = default;

ImageModel& ImageModel::operator=(ImageModel&&) = default;

// static
ImageModel ImageModel::FromVectorIcon(const gfx::VectorIcon& vector_icon,
                                      int color_id,
                                      int icon_size) {
  return ImageModel(VectorIconModel(vector_icon, color_id, icon_size));
}

// static
ImageModel ImageModel::FromVectorIcon(const gfx::VectorIcon& vector_icon,
                                      SkColor color,
                                      int icon_size) {
  return ImageModel(VectorIconModel(vector_icon, color, icon_size));
}

// static
ImageModel ImageModel::FromImage(const gfx::Image& image) {
  return ImageModel(image);
}

// static
ImageModel ImageModel::FromImageSkia(const gfx::ImageSkia& image_skia) {
  return ImageModel(image_skia);
}

bool ImageModel::IsEmpty() const {
  return !IsVectorIcon() && !IsImage();
}

bool ImageModel::IsVectorIcon() const {
  return vector_icon_model_ && !vector_icon_model_.value().is_empty();
}

bool ImageModel::IsImage() const {
  return image_ && !image_.value().IsEmpty();
}

const VectorIconModel ImageModel::GetVectorIcon() const {
  DCHECK(IsVectorIcon());
  return vector_icon_model_.value();
}

const gfx::Image ImageModel::GetImage() const {
  DCHECK(IsImage());
  return image_.value();
}

}  // namespace ui