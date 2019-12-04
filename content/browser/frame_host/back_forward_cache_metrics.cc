// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/back_forward_cache_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/sparse_histogram.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/should_swap_browsing_instance.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

// Overridden time for unit tests. Should be accessed only from the main thread.
base::TickClock* g_mock_time_clock_for_testing = nullptr;

// Reduce the resolution of the longer intervals due to privacy considerations.
base::TimeDelta ClampTime(base::TimeDelta time) {
  if (time < base::TimeDelta::FromSeconds(5))
    return base::TimeDelta::FromMilliseconds(time.InMilliseconds());
  if (time < base::TimeDelta::FromMinutes(3))
    return base::TimeDelta::FromSeconds(time.InSeconds());
  if (time < base::TimeDelta::FromHours(3))
    return base::TimeDelta::FromMinutes(time.InMinutes());
  return base::TimeDelta::FromHours(time.InHours());
}

base::TimeTicks Now() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (g_mock_time_clock_for_testing)
    return g_mock_time_clock_for_testing->NowTicks();
  return base::TimeTicks::Now();
}

}  // namespace

// static
void BackForwardCacheMetrics::OverrideTimeForTesting(base::TickClock* clock) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  g_mock_time_clock_for_testing = clock;
}

// static
scoped_refptr<BackForwardCacheMetrics>
BackForwardCacheMetrics::CreateOrReuseBackForwardCacheMetrics(
    NavigationEntryImpl* currently_committed_entry,
    bool is_main_frame_navigation,
    int64_t document_sequence_number) {
  if (!currently_committed_entry) {
    // In some rare cases it's possible to navigate a subframe
    // without having a main frame navigation (e.g. extensions
    // injecting frames into a blank page).
    return base::WrapRefCounted(new BackForwardCacheMetrics(
        is_main_frame_navigation ? document_sequence_number : -1));
  }

  BackForwardCacheMetrics* currently_committed_metrics =
      currently_committed_entry->back_forward_cache_metrics();
  if (!currently_committed_metrics) {
    // When we restore the session it's possible to end up with an entry without
    // metrics.
    // We will have to create a new metrics object for the main document.
    return base::WrapRefCounted(new BackForwardCacheMetrics(
        is_main_frame_navigation
            ? document_sequence_number
            : currently_committed_entry->root_node()
                  ->frame_entry->document_sequence_number()));
  }

  if (!is_main_frame_navigation)
    return currently_committed_metrics;
  if (document_sequence_number ==
      currently_committed_metrics->document_sequence_number_) {
    return currently_committed_metrics;
  }
  return base::WrapRefCounted(
      new BackForwardCacheMetrics(document_sequence_number));
}

BackForwardCacheMetrics::BackForwardCacheMetrics(
    int64_t document_sequence_number)
    : document_sequence_number_(document_sequence_number) {}

BackForwardCacheMetrics::~BackForwardCacheMetrics() {}

void BackForwardCacheMetrics::MainFrameDidStartNavigationToDocument() {
  if (!started_navigation_timestamp_)
    started_navigation_timestamp_ = Now();
}

void BackForwardCacheMetrics::DidCommitNavigation(
    NavigationRequest* navigation,
    bool back_forward_cache_allowed) {
  bool is_history_navigation =
      navigation->GetPageTransition() & ui::PAGE_TRANSITION_FORWARD_BACK;
  if (navigation->IsInMainFrame() && !navigation->IsSameDocument()) {
    if (is_history_navigation && back_forward_cache_allowed) {
      UpdateNotRestoredReasonsForNavigation(navigation);
      RecordMetricsForHistoryNavigationCommit(navigation);
    }
    not_restored_reasons_.reset();
    blocklisted_features_ = 0;
    disabled_reasons_.clear();
  }

  if (last_committed_main_frame_navigation_id_ != -1 &&
      navigation->IsInMainFrame()) {
    // We've visited an entry associated with this main frame document before,
    // so record metrics to determine whether it might be a back-forward cache
    // hit.
    ukm::builders::HistoryNavigation builder(ukm::ConvertToSourceId(
        navigation->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID));
    builder.SetLastCommittedSourceIdForTheSameDocument(
        ukm::ConvertToSourceId(last_committed_main_frame_navigation_id_,
                               ukm::SourceIdType::NAVIGATION_ID));
    builder.SetNavigatedToTheMostRecentEntryForDocument(
        navigation->nav_entry_id() == last_committed_navigation_entry_id_);
    builder.SetMainFrameFeatures(main_frame_features_);
    builder.SetSameOriginSubframesFeatures(same_origin_frames_features_);
    builder.SetCrossOriginSubframesFeatures(cross_origin_frames_features_);
    // DidStart notification might be missing for some same-document
    // navigations. It's good that we don't care about the time in the cache
    // in that case.
    if (started_navigation_timestamp_ &&
        navigated_away_from_main_document_timestamp_) {
      builder.SetTimeSinceNavigatedAwayFromDocument(
          ClampTime(started_navigation_timestamp_.value() -
                    navigated_away_from_main_document_timestamp_.value())
              .InMilliseconds());
    }
    builder.Record(ukm::UkmRecorder::Get());
  }
  if (navigation->IsInMainFrame())
    last_committed_main_frame_navigation_id_ = navigation->GetNavigationId();
  last_committed_navigation_entry_id_ = navigation->nav_entry_id();

  navigated_away_from_main_document_timestamp_ = base::nullopt;
  started_navigation_timestamp_ = base::nullopt;
}

void BackForwardCacheMetrics::MainFrameDidNavigateAwayFromDocument(
    RenderFrameHostImpl* new_main_frame,
    LoadCommittedDetails* details,
    NavigationRequest* navigation) {
  // MainFrameDidNavigateAwayFromDocument is called when we commit a navigation
  // to another main frame document and the current document loses its "last
  // committed" status.
  navigated_away_from_main_document_timestamp_ = Now();

  GlobalFrameRoutingId new_main_frame_id{new_main_frame->GetProcess()->GetID(),
                                         new_main_frame->GetRoutingID()};

  // If the navigation used the same RenderFrameHost, we would not be able to
  // use back-forward cache.
  if (navigation->GetPreviousRenderFrameHostId() == new_main_frame_id) {
    // Converting URLs to origins is generally discouraged [1], but here we are
    // doing this only for metrics and are not making any decisions based on
    // that.
    //
    // [1]
    // https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/origin-vs-url.md#avoid-converting-urls-to-origins
    GURL previous_site = SiteInstanceImpl::GetSiteForOrigin(
        url::Origin::Create(details->previous_url));
    GURL new_site = SiteInstanceImpl::GetSiteForOrigin(
        url::Origin::Create(navigation->GetURL()));
    if (previous_site == new_site) {
      not_restored_reasons_.set(static_cast<size_t>(
          NotRestoredReason::kRenderFrameHostReused_SameSite));
    } else {
      not_restored_reasons_.set(static_cast<size_t>(
          NotRestoredReason::kRenderFrameHostReused_CrossSite));
    }
  }
}

void BackForwardCacheMetrics::RecordFeatureUsage(
    RenderFrameHostImpl* main_frame) {
  DCHECK(!main_frame->GetParent());

  main_frame_features_ = 0;
  same_origin_frames_features_ = 0;
  cross_origin_frames_features_ = 0;

  CollectFeatureUsageFromSubtree(main_frame,
                                 main_frame->GetLastCommittedOrigin());
}

void BackForwardCacheMetrics::CollectFeatureUsageFromSubtree(
    RenderFrameHostImpl* rfh,
    const url::Origin& main_frame_origin) {
  uint64_t features = rfh->scheduler_tracked_features();
  if (!rfh->GetParent()) {
    main_frame_features_ |= features;
  } else if (rfh->GetLastCommittedOrigin().IsSameOriginWith(
                 main_frame_origin)) {
    same_origin_frames_features_ |= features;
  } else {
    cross_origin_frames_features_ |= features;
  }

  for (size_t i = 0; i < rfh->child_count(); ++i) {
    CollectFeatureUsageFromSubtree(rfh->child_at(i)->current_frame_host(),
                                   main_frame_origin);
  }
}

void BackForwardCacheMetrics::MarkNotRestoredWithReason(
    const BackForwardCacheCanStoreDocumentResult& can_store) {
  not_restored_reasons_ |= can_store.not_stored_reasons();
  blocklisted_features_ |= can_store.blocklisted_features();
  if (!browsing_instance_not_swapped_reason_) {
    browsing_instance_not_swapped_reason_ =
        can_store.browsing_instance_not_swapped_reason();
  }
  for (const std::string& reason : can_store.disabled_reasons())
    disabled_reasons_.insert(reason);
}

void BackForwardCacheMetrics::UpdateNotRestoredReasonsForNavigation(
    NavigationRequest* navigation) {
  // |last_committed_main_frame_navigation_id_| is -1 when navigation history
  // has never been initialized. This can happen only when the session history
  // has been restored.
  if (last_committed_main_frame_navigation_id_ == -1) {
    not_restored_reasons_.set(
        static_cast<size_t>(NotRestoredReason::kSessionRestored));
  }

  // This should not happen, but record this as an 'unknown' reason just in
  // case.
  if (not_restored_reasons_.none() &&
      !navigation->IsServedFromBackForwardCache()) {
    not_restored_reasons_.set(static_cast<size_t>(NotRestoredReason::kUnknown));
    // TODO(altimin): Add a (D)CHECK here, but this code is reached in
    // unittests.
    return;
  }
}

void BackForwardCacheMetrics::RecordMetricsForHistoryNavigationCommit(
    NavigationRequest* navigation) const {
  DCHECK(!navigation->IsServedFromBackForwardCache() ||
         not_restored_reasons_.none())
      << "If the navigation is served from bfcache, no not restored reasons "
         "should be recorded";

  HistoryNavigationOutcome outcome = HistoryNavigationOutcome::kNotRestored;
  if (navigation->IsServedFromBackForwardCache()) {
    outcome = HistoryNavigationOutcome::kRestored;

    UMA_HISTOGRAM_ENUMERATION(
        "BackForwardCache.EvictedAfterDocumentRestoredReason",
        BackForwardCacheMetrics::EvictedAfterDocumentRestoredReason::kRestored);
  }

  UMA_HISTOGRAM_ENUMERATION("BackForwardCache.HistoryNavigationOutcome",
                            outcome);

  for (int i = 0; i <= static_cast<int>(NotRestoredReason::kMaxValue); i++) {
    if (!not_restored_reasons_.test(static_cast<size_t>(i)))
      continue;
    DCHECK(!navigation->IsServedFromBackForwardCache());
    NotRestoredReason reason = static_cast<NotRestoredReason>(i);
    UMA_HISTOGRAM_ENUMERATION(
        "BackForwardCache.HistoryNavigationOutcome.NotRestoredReason", reason);
  }

  for (int i = 0;
       i <= static_cast<int>(
                blink::scheduler::WebSchedulerTrackedFeature::kMaxValue);
       i++) {
    blink::scheduler::WebSchedulerTrackedFeature feature =
        static_cast<blink::scheduler::WebSchedulerTrackedFeature>(i);
    if (blocklisted_features_ & blink::scheduler::FeatureToBit(feature)) {
      UMA_HISTOGRAM_ENUMERATION(
          "BackForwardCache.HistoryNavigationOutcome.BlocklistedFeature",
          feature);
    }
  }

  for (const std::string& reason : disabled_reasons_) {
    // Use SparseHistogram instead of other simple macros for metrics. It is
    // because the reasons are represented as strings, and it was impossible to
    // define an enum values.
    base::HistogramBase* histogram = base::SparseHistogram::FactoryGet(
        "BackForwardCache.HistoryNavigationOutcome."
        "DisabledForRenderFrameHostReason",
        base::HistogramBase::kUmaTargetedHistogramFlag);
    // Adopts the lower 32 bits as a signed integer from unsigned 64 bits
    // integer.
    histogram->Add(base::HistogramBase::Sample(
        static_cast<int32_t>(base::HashMetricName(reason))));
  }

  if (ShouldRecordBrowsingInstanceNotSwappedReason() &&
      browsing_instance_not_swapped_reason_) {
    UMA_HISTOGRAM_ENUMERATION(
        "BackForwardCache.HistoryNavigationOutcome."
        "BrowsingInstanceNotSwappedReason",
        browsing_instance_not_swapped_reason_.value());
  }
}

void BackForwardCacheMetrics::RecordEvictedAfterDocumentRestored(
    EvictedAfterDocumentRestoredReason reason) {
  UMA_HISTOGRAM_ENUMERATION(
      "BackForwardCache.EvictedAfterDocumentRestoredReason", reason);
}

bool BackForwardCacheMetrics::ShouldRecordBrowsingInstanceNotSwappedReason()
    const {
  for (NotRestoredReason reason :
       {NotRestoredReason::kRelatedActiveContentsExist,
        NotRestoredReason::kRenderFrameHostReused_SameSite,
        NotRestoredReason::kRenderFrameHostReused_CrossSite}) {
    if (not_restored_reasons_.test(static_cast<size_t>(reason)))
      return true;
  }
  return false;
}

}  // namespace content
