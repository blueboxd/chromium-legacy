// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/internal/image_data_store.h"

#include <utility>

#include "base/logging.h"

namespace upboarding {

namespace {

// An image data storage based on leveldb.
class ImageDataStoreImpl : public ImageDataStore {
 public:
  ImageDataStoreImpl() = default;
  ~ImageDataStoreImpl() override = default;

 private:
  // ImageDataStore implementation.
  void Init(SuccessCallback callback) override { NOTIMPLEMENTED(); }

  void Update(std::unique_ptr<ImageData> data,
              SuccessCallback callback) override {
    NOTIMPLEMENTED();
  }

  void GetImageData(const std::string& image_id,
                    ImageDataCallback callback) override {
    NOTIMPLEMENTED();
  }

  void Delete(std::vector<std::string> image_ids,
              SuccessCallback callback) override {
    NOTIMPLEMENTED();
  }
};

}  // namespace

ImageData::ImageData(const std::string& id, std::string data)
    : id_(id), data_(std::move(data)) {}

ImageData::~ImageData() = default;

void ImageData::TakeData(std::string* output) {
  DCHECK(output);
  output->swap(data_);
}

// static
std::unique_ptr<ImageDataStore> ImageDataStore::Create() {
  return std::make_unique<ImageDataStoreImpl>();
}

}  // namespace upboarding
