// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/feeds/media_feeds_service.h"

#include "base/feature_list.h"
#include "chrome/browser/media/feeds/media_feeds_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "media/base/media_switches.h"

namespace media_feeds {

MediaFeedsService::MediaFeedsService(Profile* profile) {
  DCHECK(!profile->IsOffTheRecord());
}

// static
MediaFeedsService* MediaFeedsService::Get(Profile* profile) {
  return MediaFeedsServiceFactory::GetForProfile(profile);
}

MediaFeedsService::~MediaFeedsService() = default;

bool MediaFeedsService::IsEnabled() {
  return base::FeatureList::IsEnabled(media::kMediaFeeds);
}

}  // namespace media_feeds
