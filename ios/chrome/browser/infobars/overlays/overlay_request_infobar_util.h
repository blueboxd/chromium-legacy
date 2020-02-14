// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_OVERLAY_REQUEST_INFOBAR_UTIL_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_OVERLAY_REQUEST_INFOBAR_UTIL_H_

#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_type.h"

class InfoBarIOS;
class OverlayRequest;

// Returns the InfoBarIOS used to configure |request|, or null if the InfoBarIOS
// was already destroyed or if |request| was not created with an infobar config.
InfoBarIOS* GetOverlayRequestInfobar(OverlayRequest* request);

// Returns the InfobarType of the InfoBar used to configure |request|.
// |request| must be non-null and configured with an
// InfobarOverlayRequestConfig.
// TODO(crbug.com/1038933): Remove requirements on |request| and return
// InfobarType::kNone once added.
InfobarType GetOverlayRequestInfobarType(OverlayRequest* request);

// Returns the InfobarOverlayType for |request|.  |request| must be non-null and
// configured with an InfobarOverlayRequestConfig.
InfobarOverlayType GetOverlayRequestInfobarOverlayType(OverlayRequest* request);

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_OVERLAY_REQUEST_INFOBAR_UTIL_H_
