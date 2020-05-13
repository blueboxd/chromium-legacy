// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARING_SERVICE_IMPL_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARING_SERVICE_IMPL_H_

#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

class NearbySharingServiceImpl : public NearbySharingService,
                                 public KeyedService {
 public:
  explicit NearbySharingServiceImpl(Profile* profile);
  ~NearbySharingServiceImpl() override = default;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARING_SERVICE_IMPL_H_
