// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_icon_descriptor.h"

#include <math.h>

#include "base/check_op.h"
#include "base/strings/stringprintf.h"

namespace {

int GetScalePercent(ui::ScaleFactor scale_factor) {
  return roundf(100.0f * ui::GetScaleForScaleFactor(scale_factor));
}

// Template for the icon name. First part is scale percent and second is
// resource size in dip.
constexpr char kIconNameTemplate[] = "icon_%dp_%d.png";

}  // namespace

ArcAppIconDescriptor::ArcAppIconDescriptor(int dip_size,
                                           ui::ScaleFactor scale_factor)
    : dip_size(dip_size), scale_factor(scale_factor) {
  DCHECK_GT(dip_size, 0);
  DCHECK_GT(scale_factor, ui::ScaleFactor::SCALE_FACTOR_NONE);
  DCHECK_LE(scale_factor, ui::ScaleFactor::SCALE_FACTOR_300P);
}

int ArcAppIconDescriptor::GetSizeInPixels() const {
  return roundf(dip_size * ui::GetScaleForScaleFactor(scale_factor));
}

std::string ArcAppIconDescriptor::GetName() const {
  return base::StringPrintf(kIconNameTemplate, GetScalePercent(scale_factor),
                            dip_size);
}

bool ArcAppIconDescriptor::operator==(const ArcAppIconDescriptor& other) const {
  return scale_factor == other.scale_factor && dip_size == other.dip_size;
}

bool ArcAppIconDescriptor::operator!=(const ArcAppIconDescriptor& other) const {
  return !(*this == other);
}

bool ArcAppIconDescriptor::operator<(const ArcAppIconDescriptor& other) const {
  if (dip_size != other.dip_size)
    return dip_size < other.dip_size;
  return static_cast<int>(scale_factor) < static_cast<int>(other.scale_factor);
}
