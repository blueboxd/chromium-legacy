// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_FEED_STREAM_API_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_FEED_STREAM_API_H_

#include <vector>

#include "base/observer_list_types.h"
#include "components/feed/core/v2/public/types.h"

namespace feedui {
class StreamUpdate;
}
namespace feedstore {
class DataOperation;
}

namespace feed {

// This is the public access point for interacting with the Feed stream
// contents.
class FeedStreamApi {
 public:
  class SurfaceInterface : public base::CheckedObserver {
   public:
    // Called after registering the observer to provide the full stream state.
    // Also called whenever the stream changes.
    virtual void StreamUpdate(const feedui::StreamUpdate&) = 0;
  };

  FeedStreamApi() = default;
  virtual ~FeedStreamApi() = default;

  virtual void AttachSurface(SurfaceInterface*) = 0;
  virtual void DetachSurface(SurfaceInterface*) = 0;

  virtual void SetArticlesListVisible(bool is_visible) = 0;
  virtual bool IsArticlesListVisible() = 0;

  // Apply |operations| to the stream model. Does nothing if the model is not
  // yet loaded.
  virtual void ExecuteOperations(
      std::vector<feedstore::DataOperation> operations) = 0;

  // Create a temporary change that may be undone or committed later. Does
  // nothing if the model is not yet loaded.
  virtual EphemeralChangeId CreateEphemeralChange(
      std::vector<feedstore::DataOperation> operations) = 0;
  // Commits a change. Returns false if the change does not exist.
  virtual bool CommitEphemeralChange(EphemeralChangeId id) = 0;
  // Rejects a change. Returns false if the change does not exist.
  virtual bool RejectEphemeralChange(EphemeralChangeId id) = 0;

  // User interaction reporting. These should have no side-effects other than
  // reporting metrics.
  virtual void ReportNavigationStarted() = 0;
  virtual void ReportNavigationDone() = 0;
  virtual void ReportContentRemoved() = 0;
  virtual void ReportNotInterestedIn() = 0;
  virtual void ReportManageInterests() = 0;
  virtual void ReportContextMenuOpened() = 0;
  virtual void ReportStreamScrolled(int distance_dp) = 0;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_FEED_STREAM_API_H_
