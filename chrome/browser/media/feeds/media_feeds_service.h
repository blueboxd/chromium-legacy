// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_SERVICE_H_
#define CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace media_feeds {

class MediaFeedsService : public KeyedService {
 public:
  explicit MediaFeedsService(Profile* profile);
  ~MediaFeedsService() override;
  MediaFeedsService(const MediaFeedsService& t) = delete;
  MediaFeedsService& operator=(const MediaFeedsService&) = delete;

  static bool IsEnabled();

  // Returns the instance attached to the given |profile|.
  static MediaFeedsService* Get(Profile* profile);
};

}  // namespace media_feeds

#endif  // CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_SERVICE_H_
