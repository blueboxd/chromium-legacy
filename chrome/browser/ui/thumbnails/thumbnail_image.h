// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_IMAGE_H_
#define CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_IMAGE_H_

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "ui/gfx/image/image_skia.h"

// Stores compressed thumbnail data for a tab and can vend that data as an
// uncompressed image to observers.
class ThumbnailImage : public base::RefCounted<ThumbnailImage> {
 public:
  // Smart pointer to reference-counted compressed image data; in this case
  // JPEG format.
  using CompressedThumbnailData =
      scoped_refptr<base::RefCountedData<std::vector<uint8_t>>>;

  // Observes uncompressed and/or compressed versions of the thumbnail image as
  // they are available.
  class Observer : public base::CheckedObserver {
   public:
    // Receives uncompressed thumbnail image data. Default is no-op.
    virtual void OnThumbnailImageAvailable(gfx::ImageSkia thumbnail_image);

    // Receives compressed thumbnail image data. Default is no-op.
    virtual void OnCompressedThumbnailDataAvailable(
        CompressedThumbnailData thumbnail_data);
  };

  // Represents the endpoint
  class Delegate {
   public:
    // Called whenever the thumbnail starts or stops being observed.
    // Because updating the thumbnail could be an expensive operation, it's
    // useful to track when there are no observers. Default behavior is no-op.
    virtual void ThumbnailImageBeingObservedChanged(bool is_being_observed) = 0;

   protected:
    virtual ~Delegate();

   private:
    friend class ThumbnailImage;
    ThumbnailImage* thumbnail_ = nullptr;
  };

  explicit ThumbnailImage(Delegate* delegate);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(const Observer* observer) const;

  // Sets the SkBitmap data and notifies observers with the resulting image.
  void AssignSkBitmap(SkBitmap bitmap);

  // Requests that a thumbnail image be made available to observers. Does not
  // guarantee that Observer::OnThumbnailImageAvailable() will be called, or how
  // long it will take, though in most cases it should happen very quickly.
  virtual void RequestThumbnailImage();

  void set_async_operation_finished_callback_for_testing(
      base::RepeatingClosure callback) {
    async_operation_finished_callback_ = std::move(callback);
  }

 private:
  friend class Delegate;
  friend class ThumbnailImageTest;
  friend class base::RefCounted<ThumbnailImage>;

  virtual ~ThumbnailImage();

  void AssignJPEGData(std::vector<uint8_t> data);
  bool ConvertJPEGDataToImageSkiaAndNotifyObservers();
  void NotifyUncompressedDataObservers(gfx::ImageSkia image);
  void NotifyCompressedDataObservers(CompressedThumbnailData data);

  static std::vector<uint8_t> CompressBitmap(SkBitmap bitmap);
  static gfx::ImageSkia UncompressImage(CompressedThumbnailData compressed);

  Delegate* delegate_;

  CompressedThumbnailData data_;

  base::ObserverList<Observer> observers_;

  // Called when an asynchronous operation (such as encoding image data upon
  // assignment or decoding image data for observers) finishes or fails.
  // Intended for unit tests that want to wait for internal operations following
  // AssignSkBitmap() or RequestThumbnailImage() calls.
  base::RepeatingClosure async_operation_finished_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ThumbnailImage> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ThumbnailImage);
};

#endif  // CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_IMAGE_H_
