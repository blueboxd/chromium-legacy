// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPBOARDING_QUERY_TILES_TILE_SERVICE_H_
#define CHROME_BROWSER_UPBOARDING_QUERY_TILES_TILE_SERVICE_H_

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "components/keyed_service/core/keyed_service.h"

namespace upboarding {

// The central class on chrome client responsible for fetching, storing,
// managing, and displaying query tiles in chrome.
class TileService : public KeyedService, public base::SupportsUserData {
 public:
  // TODO(shaktisahu): Add methods.

 private:
  DISALLOW_COPY_AND_ASSIGN(TileService);
};

}  // namespace upboarding

#endif  // CHROME_BROWSER_UPBOARDING_QUERY_TILES_TILE_SERVICE_H_
