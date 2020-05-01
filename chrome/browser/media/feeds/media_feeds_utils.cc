// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/feeds/media_feeds_utils.h"

#include "chrome/browser/media/feeds/media_feeds.pb.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom.h"
#include "services/media_session/public/cpp/media_image.h"

namespace media_feeds {

Image::ContentAttribute MojoContentAttributeToProto(
    media_feeds::mojom::ContentAttribute attribute) {
  switch (attribute) {
    case media_feeds::mojom::ContentAttribute::kIconic:
      return Image::ContentAttribute::Image_ContentAttribute_ICONIC;
    case media_feeds::mojom::ContentAttribute::kSceneStill:
      return Image::ContentAttribute::Image_ContentAttribute_SCENE_STILL;
    case media_feeds::mojom::ContentAttribute::kPoster:
      return Image::ContentAttribute::Image_ContentAttribute_POSTER;
    case media_feeds::mojom::ContentAttribute::kBackground:
      return Image::ContentAttribute::Image_ContentAttribute_BACKGROUND;
    case media_feeds::mojom::ContentAttribute::kForDarkBackground:
      return Image::ContentAttribute::
          Image_ContentAttribute_FOR_DARK_BACKGROUND;
    case media_feeds::mojom::ContentAttribute::kForLightBackground:
      return Image::ContentAttribute::
          Image_ContentAttribute_FOR_LIGHT_BACKGROUND;
    case media_feeds::mojom::ContentAttribute::kCentered:
      return Image::ContentAttribute::Image_ContentAttribute_CENTERED;
    case media_feeds::mojom::ContentAttribute::kRightCentered:
      return Image::ContentAttribute::Image_ContentAttribute_RIGHT_CENTERED;
    case media_feeds::mojom::ContentAttribute::kLeftCentered:
      return Image::ContentAttribute::Image_ContentAttribute_LEFT_CENTERED;
    case media_feeds::mojom::ContentAttribute::kHasTransparentBackground:
      return Image::ContentAttribute::
          Image_ContentAttribute_HAS_TRANSPARENT_BACKGROUND;
    case media_feeds::mojom::ContentAttribute::kHasTitle:
      return Image::ContentAttribute::Image_ContentAttribute_HAS_TITLE;
    case media_feeds::mojom::ContentAttribute::kNoTitle:
      return Image::ContentAttribute::Image_ContentAttribute_NO_TITLE;
    case media_feeds::mojom::ContentAttribute::kUnknown:
      return Image::ContentAttribute::
          Image_ContentAttribute_CONTENT_ATTRIBUTE_UNSPECIFIED;
  }
}

media_feeds::mojom::ContentAttribute ProtoContentAttributeToMojo(
    Image::ContentAttribute attribute) {
  switch (attribute) {
    case Image::ContentAttribute::Image_ContentAttribute_ICONIC:
      return media_feeds::mojom::ContentAttribute::kIconic;
    case Image::ContentAttribute::Image_ContentAttribute_SCENE_STILL:
      return media_feeds::mojom::ContentAttribute::kSceneStill;
    case Image::ContentAttribute::Image_ContentAttribute_POSTER:
      return media_feeds::mojom::ContentAttribute::kPoster;
    case Image::ContentAttribute::Image_ContentAttribute_BACKGROUND:
      return media_feeds::mojom::ContentAttribute::kBackground;
    case Image::ContentAttribute::Image_ContentAttribute_FOR_DARK_BACKGROUND:
      return media_feeds::mojom::ContentAttribute::kForDarkBackground;
    case Image::ContentAttribute::Image_ContentAttribute_FOR_LIGHT_BACKGROUND:
      return media_feeds::mojom::ContentAttribute::kForLightBackground;
    case Image::ContentAttribute::Image_ContentAttribute_CENTERED:
      return media_feeds::mojom::ContentAttribute::kCentered;
    case Image::ContentAttribute::Image_ContentAttribute_RIGHT_CENTERED:
      return media_feeds::mojom::ContentAttribute::kRightCentered;
    case Image::ContentAttribute::Image_ContentAttribute_LEFT_CENTERED:
      return media_feeds::mojom::ContentAttribute::kLeftCentered;
    case Image::ContentAttribute::
        Image_ContentAttribute_HAS_TRANSPARENT_BACKGROUND:
      return media_feeds::mojom::ContentAttribute::kHasTransparentBackground;
    case Image::ContentAttribute::Image_ContentAttribute_HAS_TITLE:
      return media_feeds::mojom::ContentAttribute::kHasTitle;
    case Image::ContentAttribute::Image_ContentAttribute_NO_TITLE:
      return media_feeds::mojom::ContentAttribute::kNoTitle;
    case Image::ContentAttribute::
        Image_ContentAttribute_CONTENT_ATTRIBUTE_UNSPECIFIED:
      return media_feeds::mojom::ContentAttribute::kUnknown;
    case Image::ContentAttribute::
        Image_ContentAttribute_Image_ContentAttribute_INT_MIN_SENTINEL_DO_NOT_USE_:
    case Image::ContentAttribute::
        Image_ContentAttribute_Image_ContentAttribute_INT_MAX_SENTINEL_DO_NOT_USE_:
      NOTREACHED();
      return media_feeds::mojom::ContentAttribute::kUnknown;
  }
}

void MediaImageToProto(Image* proto,
                       const media_feeds::mojom::MediaImagePtr& image) {
  if (!image)
    return;

  proto->set_url(image->src.spec());
  proto->set_width(image->size.width());
  proto->set_height(image->size.height());

  for (const auto attribute : image->content_attributes)
    proto->add_content_attribute(MojoContentAttributeToProto(attribute));
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

  for (int i = 0; i < proto.content_attribute_size(); i++) {
    image->content_attributes.push_back(
        ProtoContentAttributeToMojo(proto.content_attribute(i)));
  }

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
