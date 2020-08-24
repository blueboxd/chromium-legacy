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
#include "third_party/blink/public/mojom/page/record_content_to_visible_time_request.mojom.h"

namespace gfx {
struct PresentationFeedback;
}

namespace content {

CONTENT_EXPORT blink::mojom::RecordContentToVisibleTimeRequest& operator+=(
    blink::mojom::RecordContentToVisibleTimeRequest& a,
    blink::mojom::RecordContentToVisibleTimeRequest const& b);

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
      blink::mojom::RecordContentToVisibleTimeRequestPtr start_state,
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

  // The information about the last tab switch request, or nullptr if there is
  // no incomplete tab switch.
  blink::mojom::RecordContentToVisibleTimeRequestPtr tab_switch_start_state_;

  // The render widget visibility request timestamp for the last tab switch, or
  // null if there is no incomplete tab switch.
  base::TimeTicks render_widget_visibility_request_timestamp_;

  base::WeakPtrFactory<ContentToVisibleTimeReporter> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ContentToVisibleTimeReporter);
};

}  // namespace content

#endif  // CONTENT_COMMON_CONTENT_TO_VISIBLE_TIME_REPORTER_H_
