// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/thumbnail_image.h"

#include <utility>

#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace {

std::vector<uint8_t> SkBitmapToJPEGData(SkBitmap bitmap) {
  constexpr int kCompressionQuality = 97;
  std::vector<uint8_t> data;
  const bool result =
      gfx::JPEGCodec::Encode(bitmap, kCompressionQuality, &data);
  DCHECK(result);
  return data;
}

gfx::ImageSkia JPEGDataToImageSkia(
    scoped_refptr<base::RefCountedData<std::vector<uint8_t>>> data) {
  gfx::ImageSkia result = gfx::ImageSkia::CreateFrom1xBitmap(
      *gfx::JPEGCodec::Decode(data->data.data(), data->data.size()));
  result.MakeThreadSafe();
  return result;
}

}  // namespace

ThumbnailImage::Delegate::~Delegate() {
  if (thumbnail_)
    thumbnail_->delegate_ = nullptr;
}

ThumbnailImage::ThumbnailImage(Delegate* delegate) : delegate_(delegate) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  DCHECK(!delegate_->thumbnail_);
  delegate_->thumbnail_ = this;
}

ThumbnailImage::~ThumbnailImage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (delegate_)
    delegate_->thumbnail_ = nullptr;
}

void ThumbnailImage::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);
  if (!observers_.HasObserver(observer)) {
    const bool is_first_observer = !observers_.might_have_observers();
    observers_.AddObserver(observer);
    if (is_first_observer && delegate_)
      delegate_->ThumbnailImageBeingObservedChanged(true);
  }
}

void ThumbnailImage::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);
  if (observers_.HasObserver(observer)) {
    observers_.RemoveObserver(observer);
    if (delegate_ && !observers_.might_have_observers())
      delegate_->ThumbnailImageBeingObservedChanged(false);
  }
}

bool ThumbnailImage::HasObserver(const Observer* observer) const {
  return observers_.HasObserver(observer);
}

void ThumbnailImage::AssignSkBitmap(SkBitmap bitmap) {
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&SkBitmapToJPEGData, std::move(bitmap)),
      base::BindOnce(&ThumbnailImage::AssignJPEGData,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ThumbnailImage::RequestThumbnailImage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ConvertJPEGDataToImageSkiaAndNotifyObservers();
}

void ThumbnailImage::AssignJPEGData(std::vector<uint8_t> data) {
  data_ = base::MakeRefCounted<base::RefCountedData<std::vector<uint8_t>>>(
      std::move(data));
  ConvertJPEGDataToImageSkiaAndNotifyObservers();
}

bool ThumbnailImage::ConvertJPEGDataToImageSkiaAndNotifyObservers() {
  if (!data_) {
    if (async_operation_finished_callback_)
      async_operation_finished_callback_.Run();
    return false;
  }
  return base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&JPEGDataToImageSkia, data_),
      base::BindOnce(&ThumbnailImage::NotifyObservers,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ThumbnailImage::NotifyObservers(gfx::ImageSkia image) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (async_operation_finished_callback_)
    async_operation_finished_callback_.Run();
  for (auto& observer : observers_)
    observer.OnThumbnailImageAvailable(image);
}
