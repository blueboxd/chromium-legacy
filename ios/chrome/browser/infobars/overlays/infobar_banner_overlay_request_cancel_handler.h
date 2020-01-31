// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_INFOBAR_BANNER_OVERLAY_REQUEST_CANCEL_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_INFOBAR_BANNER_OVERLAY_REQUEST_CANCEL_HANDLER_H_

#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_cancel_handler.h"

class InfobarOverlayRequestInserter;

// A cancel handler for Infobar banner UI OverlayRequests.
class InfobarBannerOverlayRequestCancelHandler
    : public InfobarOverlayRequestCancelHandler {
 public:
  // Constructor for a handler that cancels |request| from |queue|.  |inserter|
  // is used to insert replacement requests when an infobar is replaced.
  InfobarBannerOverlayRequestCancelHandler(
      OverlayRequest* request,
      OverlayRequestQueue* queue,
      const InfobarOverlayRequestInserter* inserter);
  ~InfobarBannerOverlayRequestCancelHandler() override;

 private:
  // InfobarOverlayRequestCancelHandler:
  void HandleReplacement(infobars::InfoBar* replacement) override;

  const InfobarOverlayRequestInserter* inserter_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_INFOBAR_BANNER_OVERLAY_REQUEST_CANCEL_HANDLER_H_
