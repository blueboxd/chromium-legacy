// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_REQUEST_IMPL_H_
#define IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_REQUEST_IMPL_H_

#include <memory>

#include "ios/chrome/browser/overlays/overlay_callback_manager_impl.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"

// Internal implementation of OverlayRequest.
class OverlayRequestImpl : public OverlayRequest,
                           public base::SupportsUserData {
 public:
  OverlayRequestImpl();
  ~OverlayRequestImpl() override;

  // OverlayRequest:
  OverlayCallbackManager* GetCallbackManager() override;
  base::SupportsUserData* data() override;

 private:
  OverlayCallbackManagerImpl callback_manager_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_REQUEST_IMPL_H_
