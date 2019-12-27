// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_REQUEST_SUPPORT_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_REQUEST_SUPPORT_H_

#include "ios/chrome/browser/overlays/public/overlay_request.h"

// Helper object that allows objects to specify a subset of OverlayRequest types
// that are supported by the object.
class OverlayRequestSupport {
 public:
  virtual ~OverlayRequestSupport();

  // Whether |request| is supported by this instance.
  virtual bool IsRequestSupported(OverlayRequest* request) const;

  // Returns an OverlayRequestSupport that supports all requests.
  static const OverlayRequestSupport* All();

  // Returns an OverlayRequestSupport that does not support any requests.
  static const OverlayRequestSupport* None();

 protected:
  OverlayRequestSupport();
};

// Template used to create OverlayRequestSupports for a specific ConfigType.
template <class ConfigType>
class SupportsOverlayRequest : public OverlayRequestSupport {
 public:
  SupportsOverlayRequest() = default;
  bool IsRequestSupported(OverlayRequest* request) const override {
    return !!request->GetConfig<ConfigType>();
  }
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_REQUEST_SUPPORT_H_
