// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_TO_VISIBLE_TIME_REPORTER_H_
#define CONTENT_COMMON_CONTENT_TO_VISIBLE_TIME_REPORTER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "content/common/content_export.h"

namespace gfx {
struct PresentationFeedback;
}

namespace content {

// Keeps track of parameters for recording metrics for content to visible time
// duration for different events. Here event indicates the reason for which the
// web contents are visible. These values are set on
// RenderWidgetHostView::SetRecordContentToVisibleTimeRequest. Note that
// |show_reason_tab_switching| and |show_reason_unoccluded| can both be true at
// the same time.
struct CONTENT_EXPORT RecordContentToVisibleTimeRequest {
  RecordContentToVisibleTimeRequest();
  ~RecordContentToVisibleTimeRequest();
  RecordContentToVisibleTimeRequest(
      const RecordContentToVisibleTimeRequest& other);

  RecordContentToVisibleTimeRequest(base::TimeTicks event_start_time,
                                    base::Optional<bool> destination_is_loaded,
                                    base::Optional<bool> destination_is_frozen,
                                    bool show_reason_tab_switching,
                                    bool show_reason_unoccluded,
                                    bool show_reason_bfcache_restore);

  // Merges two requests to include all the flags set and minimum start time.
  void UpdateRequest(const RecordContentToVisibleTimeRequest& other);

  // The time at which web contents become visible.
  base::TimeTicks event_start_time = base::TimeTicks();
  // Indicates if the destination tab is loaded when initiating the tab switch.
  base::Optional<bool> destination_is_loaded;
  // Indicates if the destination tab is frozen when initiating the tab switch.
  base::Optional<bool> destination_is_frozen;
  // If |show_reason_tab_switching| is true, web contents has become visible
  // because of tab switching.
  bool show_reason_tab_switching = false;
  // If |show_reason_unoccluded| is true, then web contents has become visible
  // because window un-occlusion has happened.
  bool show_reason_unoccluded = false;
  // If |show_reason_bfcache_restore| is true, web contents has become visible
  // because of restoring a page from bfcache.
  bool show_reason_bfcache_restore = false;

  // Please update RecordContentToVisibleTimeRequestTest::ExpectEqual when
  // changing this object!!!
};

// Generates UMA metric to track the duration of tab switching from when the
// active tab is changed until the frame presentation time. The metric will be
// separated into two whether the tab switch has saved frames or not.
class CONTENT_EXPORT ContentToVisibleTimeReporter {
 public:
  // Matches the TabSwitchResult enum in enums.xml.
  enum class TabSwitchResult {
    // A frame was successfully presented after a tab switch.
    kSuccess = 0,
    // Tab was hidden before a frame was presented after a tab switch.
    kIncomplete = 1,
    // Compositor reported a failure after a tab switch.
    kPresentationFailure = 2,
    kMaxValue = kPresentationFailure,
  };

  ContentToVisibleTimeReporter();
  ~ContentToVisibleTimeReporter();

  // Invoked when the tab associated with this recorder is shown. Returns a
  // callback to invoke the next time a frame is presented for this tab.
  base::OnceCallback<void(const gfx::PresentationFeedback&)> TabWasShown(
      bool has_saved_frames,
      const RecordContentToVisibleTimeRequest& start_state,
      base::TimeTicks render_widget_visibility_request_timestamp);

  // Indicates that the tab associated with this recorder was hidden. If no
  // frame was presented since the last tab switch, failure is reported to UMA.
  void TabWasHidden();

 private:
  // Records histograms and trace events for the current tab switch.
  void RecordHistogramsAndTraceEvents(
      bool is_incomplete,
      bool show_reason_tab_switching,
      bool show_reason_unoccluded,
      bool show_reason_bfcache_restore,
      const gfx::PresentationFeedback& feedback);

  // Whether there was a saved frame for the last tab switch.
  bool has_saved_frames_;

  // The information about the last tab switch request, or nullopt if there is
  // no incomplete tab switch.
  base::Optional<RecordContentToVisibleTimeRequest> tab_switch_start_state_;

  // The render widget visibility request timestamp for the last tab switch, or
  // null if there is no incomplete tab switch.
  base::TimeTicks render_widget_visibility_request_timestamp_;

  base::WeakPtrFactory<ContentToVisibleTimeReporter> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ContentToVisibleTimeReporter);
};

}  // namespace content

#endif  // CONTENT_COMMON_CONTENT_TO_VISIBLE_TIME_REPORTER_H_
