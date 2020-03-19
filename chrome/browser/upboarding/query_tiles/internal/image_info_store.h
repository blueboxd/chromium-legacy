// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_IMAGE_INFO_STORE_H_
#define CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_IMAGE_INFO_STORE_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/time/time.h"
#include "chrome/browser/upboarding/query_tiles/internal/store.h"
#include "url/gurl.h"

namespace upboarding {

// Contains information for a query tile image. This doesn't include the raw
// bytes of the image.
// Serialized to ImageInfo protobuf in image.proto.
struct ImageInfo {
  // Unique image id.
  std::string id;

  // URL of the image.
  GURL url;

  // The most recent update time, image will be expired after a certain period
  // of time,
  base::Time last_update;
};

// Store to save query tiles image info.
class ImageInfoStore : public Store<ImageInfo> {
 public:
  ImageInfoStore() = default;
  ~ImageInfoStore() override = default;

  static std::unique_ptr<ImageInfoStore> Create();
};

}  // namespace upboarding

#endif  // CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_IMAGE_INFO_STORE_H_
