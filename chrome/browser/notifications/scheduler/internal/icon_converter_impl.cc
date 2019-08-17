// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/icon_converter_impl.h"

#include <utility>

#include "base/base64.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/notifications/scheduler/internal/icon_entry.h"
#include "ui/gfx/codec/png_codec.h"

namespace notifications {
namespace {

std::vector<IconEntry::IconData> ConvertIconToStringInternal(
    std::vector<SkBitmap> images) {
  base::AssertLongCPUWorkAllowed();
  std::vector<std::string> results;
  for (size_t i = 0; i < images.size(); i++) {
    std::vector<unsigned char> image_data;
    gfx::PNGCodec::EncodeBGRASkBitmap(
        std::move(images[i]), false /*discard_transparency*/, &image_data);
    std::string encoded(image_data.begin(), image_data.end());
    results.emplace_back(std::move(encoded));
  }
  return results;
}

std::vector<SkBitmap> ConvertStringToIconInternal(
    std::vector<std::string> encoded_data) {
  base::AssertLongCPUWorkAllowed();
  std::vector<SkBitmap> images;
  for (size_t i = 0; i < encoded_data.size(); i++) {
    SkBitmap image;
    gfx::PNGCodec::Decode(reinterpret_cast<const unsigned char*>(
                              std::move(encoded_data[i]).data()),
                          encoded_data[i].length(), &image);
    images.emplace_back(std::move(image));
  }
  return images;
}

}  // namespace

IconConverterImpl::IconConverterImpl() = default;

IconConverterImpl::~IconConverterImpl() = default;

void IconConverterImpl::ConvertIconToString(std::vector<SkBitmap> images,
                                            EncodeCallback callback) {
  DCHECK(callback);
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ConvertIconToStringInternal, std::move(images)),
      std::move(callback));
}

void IconConverterImpl::ConvertStringToIcon(
    std::vector<std::string> encoded_data,
    DecodeCallback callback) {
  DCHECK(callback);
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ConvertStringToIconInternal, std::move(encoded_data)),
      std::move(callback));
}

}  // namespace notifications
