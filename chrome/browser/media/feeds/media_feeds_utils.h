// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_UTILS_H_
#define CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_UTILS_H_

#include <vector>

#include "chrome/browser/media/feeds/media_feeds_store.mojom-forward.h"

namespace media_feeds {

class Image;
class ImageSet;

void MediaImageToProto(Image* proto,
                       const media_feeds::mojom::MediaImagePtr& image);

ImageSet MediaImagesToProto(
    const std::vector<media_feeds::mojom::MediaImagePtr>& images,
    int max_number);

media_feeds::mojom::MediaImagePtr ProtoToMediaImage(const Image& proto);

std::vector<media_feeds::mojom::MediaImagePtr> ProtoToMediaImages(
    const ImageSet& image_set,
    unsigned max_number);

}  // namespace media_feeds

#endif  // CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_UTILS_H_
