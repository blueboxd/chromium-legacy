// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_PRESENTATION_CONTEXT_OBSERVER_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_PRESENTATION_CONTEXT_OBSERVER_H_

#include "base/observer_list_types.h"

class OverlayPresentationContext;

// Observer class for the ObserverPresentationContext.
class OverlayPresentationContextObserver : public base::CheckedObserver {
 public:
  OverlayPresentationContextObserver() = default;

  // Called before |presentation_context|'s activation state changes to
  // |activating|.
  virtual void OverlayPresentationContextWillChangeActivationState(
      OverlayPresentationContext* presentation_context,
      bool activating) {}

  // Called after |presentation_context|'s activation state changes.
  virtual void OverlayPresentationContextDidChangeActivationState(
      OverlayPresentationContext* presentation_context) {}
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_PRESENTATION_CONTEXT_OBSERVER_H_
