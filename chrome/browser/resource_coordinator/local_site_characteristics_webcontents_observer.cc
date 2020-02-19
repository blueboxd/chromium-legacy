// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/local_site_characteristics_webcontents_observer.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_store_factory.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/web_contents_proxy.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace resource_coordinator {

namespace {

using LoadingState = TabLoadTracker::LoadingState;

// The period of time after loading during which we ignore title/favicon
// change events. It's possible for some site that are loaded in background to
// use some of these features without this being an attempt to communicate
// with the user (e.g. the tab is just really finishing to load).
constexpr base::TimeDelta kTitleOrFaviconChangePostLoadGracePeriod =
    base::TimeDelta::FromSeconds(20);

// The period of time during which we ignore events after a tab gets
// backgrounded. It's necessary because some events might happen shortly after
// backgrounding a tab without this being an attempt to communicate with the
// user:
//    - There might be a delay between a media request gets initiated and the
//      time the audio actually starts.
//    - Same-document navigation can cause the title or favicon to change, if
//      the user switch tab before this completes this will be recorded as a
//      background communication event while in reality it's just a navigation
//      event.
constexpr base::TimeDelta kFeatureUsagePostBackgroundGracePeriod =
    base::TimeDelta::FromSeconds(10);

performance_manager::TabVisibility ContentVisibilityToRCVisibility(
    content::Visibility visibility) {
  if (visibility == content::Visibility::VISIBLE)
    return performance_manager::TabVisibility::kForeground;
  return performance_manager::TabVisibility::kBackground;
}

}  // namespace

LocalSiteCharacteristicsWebContentsObserver::
    LocalSiteCharacteristicsWebContentsObserver(
        content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  // May not be present in some tests.
  if (performance_manager::PerformanceManagerImpl::IsAvailable()) {
    TabLoadTracker::Get()->AddObserver(this);
  }
}

LocalSiteCharacteristicsWebContentsObserver::
    ~LocalSiteCharacteristicsWebContentsObserver() {
  DCHECK(!writer_);
}

void LocalSiteCharacteristicsWebContentsObserver::OnVisibilityChanged(
    content::Visibility visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!writer_)
    return;

  auto rc_visibility = ContentVisibilityToRCVisibility(visibility);
  UpdateBackgroundedTime(rc_visibility);
  writer_->NotifySiteVisibilityChanged(rc_visibility);
}

void LocalSiteCharacteristicsWebContentsObserver::WebContentsDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (performance_manager::PerformanceManagerImpl::IsAvailable())
    TabLoadTracker::Get()->RemoveObserver(this);
  writer_.reset();
  writer_origin_ = url::Origin();
}

void LocalSiteCharacteristicsWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(navigation_handle);

  // Ignore the navigation events happening in a subframe of in the same
  // document.
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  first_time_title_set_ = false;
  first_time_favicon_set_ = false;

  if (!navigation_handle->HasCommitted())
    return;

  const url::Origin new_origin =
      url::Origin::Create(navigation_handle->GetURL());

  if (writer_ && new_origin == writer_origin_)
    return;

  writer_.reset();
  writer_origin_ = url::Origin();

  if (!URLShouldBeStoredInLocalDatabase(navigation_handle->GetURL()))
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  DCHECK(profile);
  SiteCharacteristicsDataStore* data_store =
      LocalSiteCharacteristicsDataStoreFactory::GetForProfile(profile);

  // A data store might not be available in some unit tests.
  if (data_store) {
    auto rc_visibility =
        ContentVisibilityToRCVisibility(web_contents()->GetVisibility());
    writer_ = data_store->GetWriterForOrigin(new_origin, rc_visibility);
    UpdateBackgroundedTime(rc_visibility);
  }

  // The writer is initially in an unloaded state, load it if necessary.
  if (TabLoadTracker::Get()->GetLoadingState(web_contents()) ==
      LoadingState::LOADED) {
    OnSiteLoaded();
  }

  writer_origin_ = new_origin;
}

void LocalSiteCharacteristicsWebContentsObserver::TitleWasSet(
    content::NavigationEntry* entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(sebmarchand): Check if the title is always set at least once before
  // loading completes, in which case this check could be removed.
  if (!first_time_title_set_) {
    first_time_title_set_ = true;
    return;
  }

  MaybeNotifyBackgroundFeatureUsage(
      &SiteCharacteristicsDataWriter::NotifyUpdatesTitleInBackground,
      FeatureType::kTitleChange);
}

void LocalSiteCharacteristicsWebContentsObserver::DidUpdateFaviconURL(
    const std::vector<blink::mojom::FaviconURLPtr>& candidates) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!first_time_favicon_set_) {
    first_time_favicon_set_ = true;
    return;
  }

  MaybeNotifyBackgroundFeatureUsage(
      &SiteCharacteristicsDataWriter::NotifyUpdatesFaviconInBackground,
      FeatureType::kFaviconChange);
}

void LocalSiteCharacteristicsWebContentsObserver::OnAudioStateChanged(
    bool audible) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!audible)
    return;

  MaybeNotifyBackgroundFeatureUsage(
      &SiteCharacteristicsDataWriter::NotifyUsesAudioInBackground,
      FeatureType::kAudioUsage);
}

void LocalSiteCharacteristicsWebContentsObserver::OnLoadingStateChange(
    content::WebContents* contents,
    LoadingState old_loading_state,
    LoadingState new_loading_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (web_contents() != contents)
    return;

  if (!writer_)
    return;

  // Ignore the transitions from/to an UNLOADED state.
  if (new_loading_state == LoadingState::LOADED) {
    OnSiteLoaded();
  } else if (old_loading_state == LoadingState::LOADED) {
    writer_->NotifySiteUnloaded();
    loaded_time_ = base::TimeTicks();
  }
}

bool LocalSiteCharacteristicsWebContentsObserver::ShouldIgnoreFeatureUsageEvent(
    FeatureType feature_type) {
  // The feature usage should be ignored if there's no writer for this tab.
  if (!writer_)
    return true;

  // Ignore all features happening before the website gets fully loaded.
  if (TabLoadTracker::Get()->GetLoadingState(web_contents()) !=
      LoadingState::LOADED) {
    return true;
  }

  // Ignore events if the tab is not in background.
  if (ContentVisibilityToRCVisibility(web_contents()->GetVisibility()) !=
      performance_manager::TabVisibility::kBackground) {
    return true;
  }

  if (feature_type == FeatureType::kTitleChange ||
      feature_type == FeatureType::kFaviconChange) {
    DCHECK(!loaded_time_.is_null());
    if (NowTicks() - loaded_time_ < kTitleOrFaviconChangePostLoadGracePeriod) {
      return true;
    }
  }

  // Ignore events happening shortly after the tab being backgrounded, they're
  // usually false positives.
  DCHECK(!backgrounded_time_.is_null());
  if ((NowTicks() - backgrounded_time_ <
       kFeatureUsagePostBackgroundGracePeriod)) {
    return true;
  }

  return false;
}

void LocalSiteCharacteristicsWebContentsObserver::
    MaybeNotifyBackgroundFeatureUsage(
        void (SiteCharacteristicsDataWriter::*method)(),
        FeatureType feature_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ShouldIgnoreFeatureUsageEvent(feature_type))
    return;

  (writer_.get()->*method)();
}

void LocalSiteCharacteristicsWebContentsObserver::OnSiteLoaded() {
  DCHECK(writer_);
  writer_->NotifySiteLoaded();
  loaded_time_ = NowTicks();
}

void LocalSiteCharacteristicsWebContentsObserver::UpdateBackgroundedTime(
    performance_manager::TabVisibility visibility) {
  if (visibility == performance_manager::TabVisibility::kBackground) {
    backgrounded_time_ = NowTicks();
  } else {
    backgrounded_time_ = base::TimeTicks();
  }
}

}  // namespace resource_coordinator
