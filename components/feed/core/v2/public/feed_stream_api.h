// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_FEED_STREAM_API_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_FEED_STREAM_API_H_

#include <string>
#include <vector>

#include "base/observer_list_types.h"
#include "components/feed/core/v2/public/types.h"

namespace feedui {
class StreamUpdate;
}  // namespace feedui
namespace feedstore {
class DataOperation;
}  // namespace feedstore

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

  FeedStreamApi();
  virtual ~FeedStreamApi();
  FeedStreamApi(const FeedStreamApi&) = delete;
  FeedStreamApi& operator=(const FeedStreamApi&) = delete;

  virtual void AttachSurface(SurfaceInterface*) = 0;
  virtual void DetachSurface(SurfaceInterface*) = 0;

  virtual void SetArticlesListVisible(bool is_visible) = 0;
  virtual bool IsArticlesListVisible() = 0;

  // Request to load additional content at the end of the stream.
  // Calls |callback| when complete. If no content could be added, the parameter
  // is false, and the caller should expect |LoadMore| to fail if called
  // further.
  virtual void LoadMore(base::OnceCallback<void(bool)> callback) = 0;

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

  // A slice was viewed (2/3rds of it is in the viewport). Should be called
  // once for each viewed slice in the stream.
  virtual void ReportSliceViewed(const std::string& slice_id) = 0;
  virtual void ReportNavigationStarted() = 0;
  virtual void ReportNavigationDone() = 0;
  // The user triggered the default open action, usually by tapping the card.
  virtual void ReportOpenAction(const std::string& slice_id) = 0;
  // The user triggered the 'open in new tab' action.
  virtual void ReportOpenInNewTabAction(const std::string& slice_id) = 0;
  // The user triggered the 'open in new incognito tab' action.
  virtual void ReportOpenInNewIncognitoTabAction() = 0;
  // The user pressed the 'send feedback' context menu option, but may have not
  // completed the feedback process.
  virtual void ReportSendFeedbackAction() = 0;
  // The user selected the 'learn more' option on the context menu.
  virtual void ReportLearnMoreAction() = 0;
  // The user selected the 'download' option on the context menu.
  virtual void ReportDownloadAction() = 0;
  // A piece of content was removed or dismissed explicitly by the user.
  virtual void ReportRemoveAction() = 0;
  // The 'Not Interested In' menu item was selected.
  virtual void ReportNotInterestedInAction() = 0;
  // The 'Manage Interests' menu item was selected.
  virtual void ReportManageInterestsAction() = 0;
  // The user opened the context menu (three dot, or long press).
  virtual void ReportContextMenuOpened() = 0;
  // The user scrolled the feed by |distance_dp| and then stopped.
  virtual void ReportStreamScrolled(int distance_dp) = 0;

  // The following methods are used for the internals page.

  virtual DebugStreamData GetDebugStreamData() = 0;
  // Forces a Feed refresh from the server.
  virtual void ForceRefreshForDebugging() = 0;
  // Dumps some state information for debugging.
  virtual std::string DumpStateForDebugging() = 0;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_FEED_STREAM_API_H_
