// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/feeds/media_feeds_utils.h"

#include "chrome/browser/media/feeds/media_feeds.pb.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom.h"
#include "services/media_session/public/cpp/media_image.h"

namespace media_feeds {

void MediaImageToProto(Image* proto,
                       const media_feeds::mojom::MediaImagePtr& image) {
  if (!image)
    return;

  proto->set_url(image->src.spec());
  proto->set_width(image->size.width());
  proto->set_height(image->size.height());
}

ImageSet MediaImagesToProto(
    const std::vector<media_feeds::mojom::MediaImagePtr>& images,
    int max_number) {
  ImageSet image_set;

  for (auto& image : images) {
    MediaImageToProto(image_set.add_image(), image);

    if (image_set.image().size() >= max_number)
      break;
  }

  return image_set;
}

media_feeds::mojom::MediaImagePtr ProtoToMediaImage(const Image& proto) {
  media_feeds::mojom::MediaImagePtr image =
      media_feeds::mojom::MediaImage::New();
  image->src = GURL(proto.url());
  image->size = gfx::Size(proto.width(), proto.height());

  return image;
}

std::vector<media_feeds::mojom::MediaImagePtr> ProtoToMediaImages(
    const ImageSet& image_set,
    unsigned max_number) {
  std::vector<media_feeds::mojom::MediaImagePtr> images;

  for (auto& proto : image_set.image()) {
    images.push_back(ProtoToMediaImage(proto));

    if (images.size() >= max_number)
      break;
  }

  return images;
}

}  // namespace media_feeds
