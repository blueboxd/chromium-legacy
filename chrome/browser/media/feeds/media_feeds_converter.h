// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_CONVERTER_H_
#define CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_CONVERTER_H_

#include "chrome/browser/media/feeds/media_feeds_store.mojom.h"
#include "components/schema_org/common/improved_metadata.mojom.h"

namespace media_feeds {

// Given a schema_org entity of type CompleteDataFeed, converts all items
// contained in the feed to MediaFeedItemPtr type and returns them in a vector.
// The feed should be valid according to https://wicg.github.io/media-feeds/. If
// not, GetMediaFeeds returns an empty result. If the feed is valid, but some of
// its feed items are not, GetMediaFeeds excludes the invalid feed items from
// the returned result.
base::Optional<std::vector<mojom::MediaFeedItemPtr>> GetMediaFeeds(
    schema_org::improved::mojom::EntityPtr schema_org_entity);

}  // namespace media_feeds

#endif  // CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_CONVERTER_H_
