// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/internal/image_info_store.h"

#include <utility>

#include "base/logging.h"

namespace upboarding {
namespace {

class ImageInfoStoreImpl : public ImageInfoStore {
 public:
  ImageInfoStoreImpl() = default;
  ~ImageInfoStoreImpl() override = default;

 private:
  void InitAndLoad(Store::LoadCallback callback) override { NOTIMPLEMENTED(); }

  void Update(const std::string& key,
              const ImageInfo& entry,
              Store::UpdateCallback callback) override {
    NOTIMPLEMENTED();
  }

  void Delete(const std::string& key, Store::DeleteCallback callback) override {
    NOTIMPLEMENTED();
  }
};

}  // namespace

// static
std::unique_ptr<ImageInfoStore> ImageInfoStore::Create() {
  return std::make_unique<ImageInfoStoreImpl>();
}

}  // namespace upboarding
