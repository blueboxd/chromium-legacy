// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_data.h"

#include <ostream>

#include "base/notreached.h"
#include "skia/ext/skia_utils_base.h"
#include "ui/gfx/skia_util.h"

namespace ui {

ClipboardData::ClipboardData() : web_smart_paste_(false), format_(0) {}

ClipboardData::ClipboardData(const ClipboardData&) = default;

ClipboardData::~ClipboardData() = default;

bool ClipboardData::operator==(const ClipboardData& that) const {
  return format_ == that.format() && text_ == that.text() &&
         markup_data_ == that.markup_data() && url_ == that.url() &&
         rtf_data_ == that.rtf_data() &&
         bookmark_title_ == that.bookmark_title() &&
         bookmark_url_ == that.bookmark_url() &&
         custom_data_format_ == that.custom_data_format() &&
         custom_data_data_ == that.custom_data_data() &&
         web_smart_paste_ == that.web_smart_paste() &&
         gfx::BitmapsAreEqual(bitmap_, that.bitmap());
}

void ClipboardData::SetBitmapData(const SkBitmap& bitmap) {
  if (!skia::SkBitmapToN32OpaqueOrPremul(bitmap, &bitmap_)) {
    NOTREACHED() << "Unable to convert bitmap for clipboard";
    return;
  }
  format_ |= static_cast<int>(ClipboardInternalFormat::kBitmap);
}

void ClipboardData::SetCustomData(const std::string& data_format,
                                  const std::string& data_data) {
  if (data_data.size() == 0) {
    custom_data_data_.clear();
    custom_data_format_.clear();
    return;
  }
  custom_data_data_ = data_data;
  custom_data_format_ = data_format;
  format_ |= static_cast<int>(ClipboardInternalFormat::kCustom);
}

}  // namespace ui
