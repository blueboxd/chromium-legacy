// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_INFOBAR_BANNER_OVERLAY_RESPONSES_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_INFOBAR_BANNER_OVERLAY_RESPONSES_H_

#include "ios/chrome/browser/overlays/public/overlay_response_info.h"

// Response info used to create dispatched OverlayResponses that trigger the
// infobar's main action.
class InfobarBannerMainActionResponse
    : public OverlayResponseInfo<InfobarBannerMainActionResponse> {
 public:
  ~InfobarBannerMainActionResponse() override;

 private:
  OVERLAY_USER_DATA_SETUP(InfobarBannerMainActionResponse);
  InfobarBannerMainActionResponse();
};

// Response info used to create dispatched OverlayResponses that trigger the
// presentation of the infobar's modal.
class InfobarBannerShowModalResponse
    : public OverlayResponseInfo<InfobarBannerShowModalResponse> {
 public:
  ~InfobarBannerShowModalResponse() override;

 private:
  OVERLAY_USER_DATA_SETUP(InfobarBannerShowModalResponse);
  InfobarBannerShowModalResponse();
};

// Response info used to create dispatched OverlayResponses that notify the
// model layer that the upcoming dismissal is user-initiated (i.e. swipe up to
// dismiss the banner on the refresh banner UI).
class InfobarBannerUserInitiatedDismissalResponse
    : public OverlayResponseInfo<InfobarBannerUserInitiatedDismissalResponse> {
 public:
  ~InfobarBannerUserInitiatedDismissalResponse() override;

 private:
  OVERLAY_USER_DATA_SETUP(InfobarBannerUserInitiatedDismissalResponse);
  InfobarBannerUserInitiatedDismissalResponse();
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_INFOBAR_BANNER_OVERLAY_RESPONSES_H_
