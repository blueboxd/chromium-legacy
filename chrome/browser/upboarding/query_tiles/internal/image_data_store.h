// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_IMAGE_DATA_STORE_H_
#define CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_IMAGE_DATA_STORE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"

namespace upboarding {

// Contains decoded image data.
// Serialized to ImageData protobuf in image.proto.
class ImageData {
 public:
  ImageData(const std::string& id, std::string data);
  ImageData(const ImageData&) = delete;
  ImageData& operator=(const ImageData&) = delete;
  ~ImageData();

  const std::string& id() const { return id_; }

  // Transfers the ownership of |data_|.
  void TakeData(std::string* output);

 private:
  // Unique id of the image.
  std::string id_;

  // Raw bytes of the image.
  std::string data_;
};

// Storage to save deoded query tile images' raw data.
// Only supports loads one image at a time.
class ImageDataStore {
 public:
  using SuccessCallback = base::OnceCallback<void(bool /*success*/)>;
  using ImageDataCallback =
      base::OnceCallback<void(std::unique_ptr<ImageData>)>;

  static std::unique_ptr<ImageDataStore> Create();

  ImageDataStore() = default;
  ImageDataStore(const ImageDataStore&) = delete;
  ImageDataStore& operator=(const ImageDataStore&) = delete;
  virtual ~ImageDataStore() = default;

  // Initializes the store.
  virtual void Init(SuccessCallback callback) = 0;

  // Updates one image.
  virtual void Update(std::unique_ptr<ImageData> data,
                      SuccessCallback callback) = 0;

  // Loads one image data into memory.
  virtual void GetImageData(const std::string& image_id,
                            ImageDataCallback callback) = 0;

  // Deletes images from the store.
  virtual void Delete(std::vector<std::string> image_ids,
                      SuccessCallback callback) = 0;
};

}  // namespace upboarding

#endif  // CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_IMAGE_DATA_STORE_H_
