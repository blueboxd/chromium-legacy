// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lite_video/lite_video_decider.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/optional.h"
#include "chrome/browser/lite_video/lite_video_features.h"
#include "chrome/browser/lite_video/lite_video_hint.h"
#include "chrome/browser/lite_video/lite_video_hint_cache.h"
#include "chrome/browser/lite_video/lite_video_user_blocklist.h"
#include "components/blocklist/opt_out_blocklist/opt_out_store.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace {

// Utility class for recording the decision of whether LiteVideos should be
// applied to a navigation and if a LiteVideoHint is available for the
// navigation. The result is recorded when it goes out of scope and its
// destructor is called.
class ScopedLiteVideoDecisionRecorder {
 public:
  explicit ScopedLiteVideoDecisionRecorder(
      lite_video::LiteVideoBlocklistReason blocklist_reason,
      bool is_mainframe)
      : blocklist_reason_(blocklist_reason),
        is_mainframe_(is_mainframe),
        has_hint_for_host_(false) {}
  ~ScopedLiteVideoDecisionRecorder() {
    if (is_mainframe_) {
      UMA_HISTOGRAM_ENUMERATION(
          "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame",
          blocklist_reason_);
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame",
          blocklist_reason_);
    }
    UMA_HISTOGRAM_BOOLEAN("LiteVideo.CanApplyLiteVideo.HintCache.HasHint",
                          has_hint_for_host_);
  }
  void set_has_hint_for_host(bool has_hint_for_host) {
    has_hint_for_host_ = has_hint_for_host;
  }

 private:
  lite_video::LiteVideoBlocklistReason blocklist_reason_;
  bool is_mainframe_;
  bool has_hint_for_host_;
};

}  // namespace

namespace lite_video {

LiteVideoDecider::LiteVideoDecider(
    std::unique_ptr<blocklist::OptOutStore> opt_out_store,
    base::Clock* clock)
    : hint_cache_(std::make_unique<LiteVideoHintCache>()) {
  user_blocklist_ = std::make_unique<LiteVideoUserBlocklist>(
      std::move(opt_out_store), clock, this);
}

LiteVideoDecider::~LiteVideoDecider() = default;

base::Optional<LiteVideoHint> LiteVideoDecider::CanApplyLiteVideo(
    content::NavigationHandle* navigation_handle) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!features::IsLiteVideoEnabled())
    return base::nullopt;

  // TODO(crbug/1096193): Add checks for Lite mode and ECT.

  // TODO(crbug/1096796): Add checks on the page transition and update
  // the blocklist if needed page transition.

  GURL url = navigation_handle->GetURL();

  if (!url.SchemeIsHTTPOrHTTPS())
    return base::nullopt;

  auto blocklist_reason =
      user_blocklist_->IsLiteVideoAllowedOnNavigation(navigation_handle);
  ScopedLiteVideoDecisionRecorder scoped_decision_recorder(
      blocklist_reason, navigation_handle->IsInMainFrame());

  base::Optional<LiteVideoHint> hint =
      hint_cache_->GetHintForNavigationURL(url);
  if (hint)
    scoped_decision_recorder.set_has_hint_for_host(true);

  if (blocklist_reason != LiteVideoBlocklistReason::kAllowed || !hint)
    return base::nullopt;

  return hint;
}

void LiteVideoDecider::OnUserBlocklistedStatusChange(bool blocklisted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!blocklist_loaded_) {
    blocklist_loaded_ = true;
    // Local event used as a signal for testing.
    LOCAL_HISTOGRAM_BOOLEAN("LiteVideo.UserBlocklist.BlocklistLoaded", true);
  }
}

}  // namespace lite_video
