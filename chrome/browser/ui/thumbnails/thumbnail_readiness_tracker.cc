// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/thumbnail_readiness_tracker.h"

#include <utility>

#include "content/public/browser/navigation_handle.h"

ThumbnailReadinessTracker::ThumbnailReadinessTracker(
    content::WebContents* web_contents,
    ReadinessChangeCallback callback)
    : content::WebContentsObserver(web_contents),
      callback_(std::move(callback)) {
  DCHECK(callback_);
}

ThumbnailReadinessTracker::~ThumbnailReadinessTracker() = default;

void ThumbnailReadinessTracker::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;
  UpdateReadiness(Readiness::kNotReady);
}

void ThumbnailReadinessTracker::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;
  if (last_readiness_ > Readiness::kReadyForInitialCapture)
    return;
  UpdateReadiness(Readiness::kReadyForInitialCapture);
}

void ThumbnailReadinessTracker::DocumentOnLoadCompletedInMainFrame() {
  UpdateReadiness(Readiness::kReadyForFinalCapture);
}

void ThumbnailReadinessTracker::WebContentsDestroyed() {
  UpdateReadiness(Readiness::kNotReady);
}

void ThumbnailReadinessTracker::UpdateReadiness(Readiness readiness) {
  if (readiness == last_readiness_)
    return;

  // If the WebContents is closing, it shouldn't be captured.
  if (web_contents()->IsBeingDestroyed())
    readiness = Readiness::kNotReady;

  last_readiness_ = readiness;
  callback_.Run(readiness);
}
