// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_CONVERTER_H_
#define CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_CONVERTER_H_

#include "chrome/browser/media/feeds/media_feeds_store.mojom.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "components/schema_org/common/improved_metadata.mojom.h"

namespace media_feeds {

// Given a schema_org entity of type CompleteDataFeed, converts all items
// contained in the feed to MediaFeedItemPtr type and places them in the outputs
// nested items vector.
//
// The feed should be valid according to https://wicg.github.io/media-feeds/. If
// not, ConvertMediaFeed does not populate any fields and returns false. If the
// feed is valid, but some of its feed items are not, ConvertMediaFeed excludes
// the invalid feed items from the result.
bool ConvertMediaFeed(
    const schema_org::improved::mojom::EntityPtr& schema_org_entity,
    media_history::MediaHistoryKeyedService::MediaFeedFetchResult* result);

}  // namespace media_feeds

#endif  // CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_CONVERTER_H_
