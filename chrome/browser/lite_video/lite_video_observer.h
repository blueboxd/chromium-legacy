// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_OBSERVER_H_
#define CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_OBSERVER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "chrome/browser/lite_video/lite_video_user_blocklist.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace lite_video {
class LiteVideoDecider;
class LiteVideoHint;

// The decision if a navigation should attempt to throttle media requests.
// This should be kept in sync with LiteVideoDecision in enums.xml.
enum class LiteVideoDecision {
  kUnknown,
  // The navigation is allowed by all types of this LiteVideoUserBlocklist.
  kAllowed,
  // The navigation is not allowed by all types of this LiteVideoUserBlocklist.
  kNotAllowed,
  // The navigation is allowed by all types of this LiteVideoUserBlocklist but
  // the optimization was heldback for counterfactual experiments.
  kHoldback,

  // Insert new values before this line.
  kMaxValue = kHoldback,
};

}  // namespace lite_video

class LiteVideoObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<LiteVideoObserver> {
 public:
  static void MaybeCreateForWebContents(content::WebContents* web_contents);

  ~LiteVideoObserver() override;

 private:
  friend class content::WebContentsUserData<LiteVideoObserver>;
  explicit LiteVideoObserver(content::WebContents* web_contents);

  // content::WebContentsObserver.
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Determines the LiteVideoDecision based on |hint| and the coinflip
  // holdback state.
  lite_video::LiteVideoDecision MakeLiteVideoDecision(
      content::NavigationHandle* navigation_handle,
      base::Optional<lite_video::LiteVideoHint> hint) const;

  // Records the metrics for LiteVideos applied to any frames associated with
  // the current mainframe navigation id. Called once per frame. Also, called
  // for frames in the same document navigations.
  void RecordUKMMetrics(lite_video::LiteVideoDecision decision,
                        lite_video::LiteVideoBlocklistReason blocklist_reason);

  // The decider capable of making decisions about whether LiteVideos should be
  // applied and the params to use when throttling media requests.
  lite_video::LiteVideoDecider* lite_video_decider_ = nullptr;

  // The current navigation id of the mainframe navigation being observed. Used
  // for recording tying all UKM metrics to the mainframe navigation source.
  base::Optional<int64_t> current_mainframe_navigation_id_;

  // Whether the navigations currently being observed should have the LiteVideo
  // optimization heldback due to a coinflip, counterfactual experiment.
  // |is_coinflip_holdback_| is updated each time a mainframe navigation
  // commits.
  bool is_coinflip_holdback_ = false;

  // True if the main frame was not eligible for LiteVideo.
  bool ineligible_main_frame_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_OBSERVER_H_
