// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lite_video/lite_video_decider.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/optional.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lite_video/lite_video_features.h"
#include "chrome/browser/lite_video/lite_video_hint.h"
#include "chrome/browser/lite_video/lite_video_hint_cache.h"
#include "chrome/browser/lite_video/lite_video_user_blocklist.h"
#include "chrome/browser/lite_video/lite_video_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/blocklist/opt_out_blocklist/opt_out_store.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"

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

  network::NetworkQualityTracker* nqe_tracker =
      g_browser_process->network_quality_tracker();
  if (nqe_tracker) {
    nqe_tracker->AddEffectiveConnectionTypeObserver(this);
    current_effective_connection_type_ =
        nqe_tracker->GetEffectiveConnectionType();
  }

  network::NetworkConnectionTracker* network_connection_tracker =
      content::GetNetworkConnectionTracker();
  if (network_connection_tracker) {
    network_connection_tracker->AddNetworkConnectionObserver(this);
    network::mojom::ConnectionType connection_type =
        network::mojom::ConnectionType::CONNECTION_UNKNOWN;
    network_connection_tracker->GetConnectionType(&connection_type,
                                                  base::DoNothing());
    is_cellular_network_ =
        network_connection_tracker->IsConnectionCellular(connection_type);
  }
}

LiteVideoDecider::~LiteVideoDecider() {
  g_browser_process->network_quality_tracker()
      ->RemoveEffectiveConnectionTypeObserver(this);
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
}

base::Optional<LiteVideoHint> LiteVideoDecider::CanApplyLiteVideo(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsLiteVideoAllowedForUser(Profile::FromBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext()))) {
    return base::nullopt;
  }

  if (!is_cellular_network_)
    return base::nullopt;

  if (current_effective_connection_type_ < features::MinLiteVideoECT())
    return base::nullopt;

  GURL url = navigation_handle->GetURL();

  if (!url.SchemeIsHTTPOrHTTPS())
    return base::nullopt;

  // Reloads and Forward-Back navigations are considered opt-outs and are added
  // to the blocklist so that a host that is frequently reloaded on does not get
  // LiteVideos.
  bool is_reload = PageTransitionCoreTypeIs(
      navigation_handle->GetPageTransition(), ui::PAGE_TRANSITION_RELOAD);
  if (is_reload || (navigation_handle->GetPageTransition() &
                    ui::PAGE_TRANSITION_FORWARD_BACK)) {
    user_blocklist_->AddNavigationToBlocklist(navigation_handle, true);
    ScopedLiteVideoDecisionRecorder scoped_decision_recorder(
        is_reload ? LiteVideoBlocklistReason::kNavigationReload
                  : LiteVideoBlocklistReason::kNavigationForwardBack,
        navigation_handle->IsInMainFrame());
    return base::nullopt;
  }

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

  // The navigation will have the LiteVideo optimization triggered so
  // update the navigation blocklist.
  user_blocklist_->AddNavigationToBlocklist(navigation_handle, false);
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

void LiteVideoDecider::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType effective_connection_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_effective_connection_type_ = effective_connection_type;
}

void LiteVideoDecider::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  is_cellular_network_ =
      network::NetworkConnectionTracker::IsConnectionCellular(type);
}

}  // namespace lite_video
