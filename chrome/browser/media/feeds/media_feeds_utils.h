// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_UTILS_H_
#define CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_UTILS_H_

#include <vector>

namespace media_session {
struct MediaImage;
}  // namespace media_session

namespace media_feeds {

class Image;
class ImageSet;

void MediaImageToProto(Image* proto, const media_session::MediaImage& image);

ImageSet MediaImagesToProto(
    const std::vector<media_session::MediaImage>& images,
    int max_number);

media_session::MediaImage ProtoToMediaImage(const Image& proto);

std::vector<media_session::MediaImage> ProtoToMediaImages(
    const ImageSet& image_set,
    unsigned max_number);

}  // namespace media_feeds

#endif  // CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_UTILS_H_
