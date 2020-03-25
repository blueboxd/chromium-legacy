// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/feeds/media_feeds_utils.h"

#include "chrome/browser/media/feeds/media_feeds.pb.h"
#include "services/media_session/public/cpp/media_image.h"

namespace media_feeds {

void MediaImageToProto(Image* proto, const media_session::MediaImage& image) {
  proto->set_url(image.src.spec());

  if (image.sizes.empty())
    return;

  DCHECK_EQ(1u, image.sizes.size());

  proto->set_width(image.sizes[0].width());
  proto->set_height(image.sizes[0].height());
}

ImageSet MediaImagesToProto(
    const std::vector<media_session::MediaImage>& images,
    int max_number) {
  ImageSet image_set;

  for (auto& image : images) {
    MediaImageToProto(image_set.add_image(), image);

    if (image_set.image().size() >= max_number)
      break;
  }

  return image_set;
}

media_session::MediaImage ProtoToMediaImage(const Image& proto) {
  media_session::MediaImage image;
  image.src = GURL(proto.url());

  if (proto.width() > 0 && proto.height() > 0)
    image.sizes.push_back(gfx::Size(proto.width(), proto.height()));

  return image;
}

std::vector<media_session::MediaImage> ProtoToMediaImages(
    const ImageSet& image_set,
    unsigned max_number) {
  std::vector<media_session::MediaImage> images;

  for (auto& proto : image_set.image()) {
    images.push_back(ProtoToMediaImage(proto));

    if (images.size() >= max_number)
      break;
  }

  return images;
}

}  // namespace media_feeds
