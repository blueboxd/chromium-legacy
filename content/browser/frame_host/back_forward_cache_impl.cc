// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/back_forward_cache_impl.h"

#include <algorithm>
#include <string>

#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/page_messages.h"
#include "content/public/common/content_features.h"
#include "content/public/common/navigation_policy.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"
#if defined(OS_ANDROID)
#include "content/public/browser/android/child_process_importance.h"
#endif

namespace content {

namespace {

using blink::scheduler::WebSchedulerTrackedFeature;

// Removes the time limit for cached content. This is used on bots to identify
// accidentally passing tests.
const base::Feature kBackForwardCacheNoTimeEviction{
    "BackForwardCacheNoTimeEviction", base::FEATURE_DISABLED_BY_DEFAULT};

// The number of entries the BackForwardCache can hold per tab.
static constexpr size_t kBackForwardCacheLimit = 1;

// The default time to live in seconds for documents in BackForwardCache.
static constexpr int kDefaultTimeToLiveInBackForwardCacheInSeconds = 15;

#if defined(OS_ANDROID)
bool IsProcessBindingEnabled() {
  const std::string process_binding_param =
      base::GetFieldTrialParamValueByFeature(features::kBackForwardCache,
                                             "process_binding_strength");
  return process_binding_param.empty() || process_binding_param == "DISABLE";
}

// Association of ChildProcessImportance to corresponding string names.
const base::FeatureParam<ChildProcessImportance>::Option
    child_process_importance_options[] = {
        {ChildProcessImportance::IMPORTANT, "IMPORTANT"},
        {ChildProcessImportance::MODERATE, "MODERATE"},
        {ChildProcessImportance::NORMAL, "NORMAL"}};

// Defines the binding strength for a processes holding cached pages. The value
// is read from an experiment parameter value. Ideally this would be lower than
// the one for processes holding the foreground page and similar to that of
// background tabs so that the OS will hopefully kill the foreground tab last.
// The default importance is set to MODERATE.
const base::FeatureParam<ChildProcessImportance> kChildProcessImportanceParam{
    &features::kBackForwardCache, "process_binding_strength",
    ChildProcessImportance::MODERATE, &child_process_importance_options};
#endif

bool IsServiceWorkerSupported() {
  static constexpr base::FeatureParam<bool> service_worker_supported(
      &features::kBackForwardCache, "service_worker_supported", false);
  return service_worker_supported.Get();
}

bool IsGeolocationSupported() {
  static constexpr base::FeatureParam<bool> geolocation_supported(
      &features::kBackForwardCache, "geolocation_supported",
#if defined(OS_ANDROID)
      true
#else
      // TODO(crbug.com/989847): Omnibox icon should be updated when the page
      //                         enters or is restored from BackForwardCache.
      false
#endif
  );
  return geolocation_supported.Get();
}

// Ignore all features that the page is using and all DisableForRenderFrameHost
// calls and force all pages to be cached. Should be used only for local testing
// and debugging -- things will break when this param is used.
bool ShouldIgnoreBlocklists() {
  static constexpr base::FeatureParam<bool> should_ignore_blocklists(
      &features::kBackForwardCache, "should_ignore_blocklists", false);
  return should_ignore_blocklists.Get();
}

uint64_t GetDisallowedFeatures(RenderFrameHostImpl* rfh) {
  // TODO(https://crbug.com/1015784): Finalize disallowed feature list, and test
  // for each disallowed feature.
  constexpr uint64_t kAlwaysDisallowedFeatures =
      FeatureToBit(WebSchedulerTrackedFeature::kWebSocket) |
      FeatureToBit(WebSchedulerTrackedFeature::kWebRTC) |
      FeatureToBit(WebSchedulerTrackedFeature::kContainsPlugins) |
      FeatureToBit(WebSchedulerTrackedFeature::kDedicatedWorkerOrWorklet) |
      FeatureToBit(WebSchedulerTrackedFeature::kOutstandingNetworkRequest) |
      FeatureToBit(
          WebSchedulerTrackedFeature::kOutstandingIndexedDBTransaction) |
      FeatureToBit(
          WebSchedulerTrackedFeature::kHasScriptableFramesInMultipleTabs) |
      FeatureToBit(
          WebSchedulerTrackedFeature::kRequestedNotificationsPermission) |
      FeatureToBit(WebSchedulerTrackedFeature::kRequestedMIDIPermission) |
      FeatureToBit(
          WebSchedulerTrackedFeature::kRequestedAudioCapturePermission) |
      FeatureToBit(
          WebSchedulerTrackedFeature::kRequestedVideoCapturePermission) |
      FeatureToBit(WebSchedulerTrackedFeature::
                       kRequestedBackForwardCacheBlockedSensors) |
      FeatureToBit(
          WebSchedulerTrackedFeature::kRequestedBackgroundWorkPermission) |
      FeatureToBit(WebSchedulerTrackedFeature::kBroadcastChannel) |
      FeatureToBit(WebSchedulerTrackedFeature::kIndexedDBConnection) |
      FeatureToBit(WebSchedulerTrackedFeature::kWebGL) |
      FeatureToBit(WebSchedulerTrackedFeature::kWebVR) |
      FeatureToBit(WebSchedulerTrackedFeature::kWebXR) |
      FeatureToBit(WebSchedulerTrackedFeature::kSharedWorker) |
      FeatureToBit(WebSchedulerTrackedFeature::kWebXR) |
      FeatureToBit(WebSchedulerTrackedFeature::kWebLocks);

  uint64_t result = kAlwaysDisallowedFeatures;

  if (!IsServiceWorkerSupported()) {
    result |=
        FeatureToBit(WebSchedulerTrackedFeature::kServiceWorkerControlledPage);
  }

  if (!IsGeolocationSupported()) {
    result |= FeatureToBit(
        WebSchedulerTrackedFeature::kRequestedGeolocationPermission);
  }

  // We do not cache documents which have cache-control: no-store header on
  // their main resource.
  if (!rfh->GetParent()) {
    result |= FeatureToBit(
        WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoStore);
  }

  return result;
}

// The BackForwardCache feature is controlled via an experiment. This function
// returns the allowed URLs where it is enabled. To enter the BackForwardCache
// the URL of a document must have a host and a path matching with at least
// one URL in this map. We represent/format the string associated with
// parameter as comma separated urls.
std::map<std::string, std::vector<std::string>> SetAllowedURLs() {
  std::map<std::string, std::vector<std::string>> allowed_urls;
  for (auto& it :
       base::SplitString(base::GetFieldTrialParamValueByFeature(
                             features::kBackForwardCache, "allowed_websites"),
                         ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    GURL url = GURL(it);
    allowed_urls[url.host()].emplace_back(url.path());
  }
  return allowed_urls;
}

BackForwardCacheTestDelegate* g_bfcache_disabled_test_observer = nullptr;

void RestoreBrowserControlsState(RenderFrameHostImpl* cached_rfh) {
  auto* current_rfh =
      cached_rfh->frame_tree_node()->render_manager()->current_frame_host();

  DCHECK_NE(current_rfh, cached_rfh);

  float prev_top_controls_shown_ratio = current_rfh->GetRenderWidgetHost()
                                            ->render_frame_metadata_provider()
                                            ->LastRenderFrameMetadata()
                                            .top_controls_shown_ratio;
  if (prev_top_controls_shown_ratio < 1) {
    // Make sure the state in the restored renderer matches the current one.
    // If we currently aren't showing the controls let the cached renderer
    // know, so that it then reacts correctly to the SHOW controls message
    // that might follow during DidCommitNavigation.
    cached_rfh->UpdateBrowserControlsState(
        BrowserControlsState::BROWSER_CONTROLS_STATE_BOTH,
        BrowserControlsState::BROWSER_CONTROLS_STATE_HIDDEN,
        // Do not animate as we want this to happen "instantaneously"
        false);
  }
}

}  // namespace

BackForwardCacheImpl::Entry::Entry(
    std::unique_ptr<RenderFrameHostImpl> rfh,
    RenderFrameProxyHostMap proxies,
    std::set<RenderViewHostImpl*> render_view_hosts)
    : render_frame_host(std::move(rfh)),
      proxy_hosts(std::move(proxies)),
      render_view_hosts(std::move(render_view_hosts)) {}
BackForwardCacheImpl::Entry::~Entry() {}

BackForwardCacheTestDelegate::BackForwardCacheTestDelegate() {
  DCHECK(!g_bfcache_disabled_test_observer);
  g_bfcache_disabled_test_observer = this;
}

BackForwardCacheTestDelegate::~BackForwardCacheTestDelegate() {
  DCHECK_EQ(g_bfcache_disabled_test_observer, this);
  g_bfcache_disabled_test_observer = nullptr;
}

BackForwardCacheImpl::BackForwardCacheImpl()
    : allowed_urls_(SetAllowedURLs()), weak_factory_(this) {}
BackForwardCacheImpl::~BackForwardCacheImpl() = default;

base::TimeDelta BackForwardCacheImpl::GetTimeToLiveInBackForwardCache() {
  // We use the following order of priority if multiple values exist:
  // - The programmatical value set in params. Used in specific tests.
  // - Infinite if kBackForwardCacheNoTimeEviction is enabled.
  // - Default value otherwise, kDefaultTimeToLiveInBackForwardCacheInSeconds.
  if (base::FeatureList::IsEnabled(kBackForwardCacheNoTimeEviction) &&
      GetFieldTrialParamValueByFeature(features::kBackForwardCache,
                                       "TimeToLiveInBackForwardCacheInSeconds")
          .empty()) {
    return base::TimeDelta::Max();
  }

  return base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
      features::kBackForwardCache, "TimeToLiveInBackForwardCacheInSeconds",
      kDefaultTimeToLiveInBackForwardCacheInSeconds));
}

BackForwardCacheCanStoreDocumentResult BackForwardCacheImpl::CanStoreDocument(
    RenderFrameHostImpl* rfh) {
  BackForwardCacheCanStoreDocumentResult result;

  // Use the BackForwardCache only for the main frame.
  if (rfh->GetParent())
    result.No(BackForwardCacheMetrics::NotRestoredReason::kNotMainFrame);

  if (!IsBackForwardCacheEnabled() || is_disabled_for_testing_) {
    result.No(
        BackForwardCacheMetrics::NotRestoredReason::kBackForwardCacheDisabled);
  }

  // Two pages in the same BrowsingInstance can script each other. When a page
  // can be scripted from outside, it can't enter the BackForwardCache.
  //
  // The "RelatedActiveContentsCount" below is compared against 0, not 1. This
  // is because the |rfh| is not a "current" RenderFrameHost anymore. It is not
  // "active" itself.
  //
  // This check makes sure the old and new document aren't sharing the same
  // BrowsingInstance.
  if (rfh->GetSiteInstance()->GetRelatedActiveContentsCount() != 0) {
    result.NoDueToRelatedActiveContents(
        rfh->browsing_instance_not_swapped_reason());
  }

  // Only store documents that have successful http status code.
  // Note that for error pages, |last_http_status_code| is equal to 0.
  if (rfh->last_http_status_code() != net::HTTP_OK)
    result.No(BackForwardCacheMetrics::NotRestoredReason::kHTTPStatusNotOK);

  // Only store documents that were fetched via HTTP GET method.
  if (rfh->last_http_method() != net::HttpRequestHeaders::kGetMethod)
    result.No(BackForwardCacheMetrics::NotRestoredReason::kHTTPMethodNotGET);

  // Do not store main document with non HTTP/HTTPS URL scheme. In particular,
  // this excludes the new tab page.
  if (!rfh->GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
    result.No(
        BackForwardCacheMetrics::NotRestoredReason::kSchemeNotHTTPOrHTTPS);
  }

  // Only store documents that have URLs allowed through experiment.
  if (!IsAllowed(rfh->GetLastCommittedURL()))
    result.No(BackForwardCacheMetrics::NotRestoredReason::kDomainNotAllowed);

  CanStoreRenderFrameHost(&result, rfh);

  DVLOG(1) << "CanStoreDocument: " << rfh->GetLastCommittedURL() << " : "
           << result.ToString();
  return result;
}

// Recursively checks whether this RenderFrameHost and all child frames
// can be cached.
void BackForwardCacheImpl::CanStoreRenderFrameHost(
    BackForwardCacheCanStoreDocumentResult* result,
    RenderFrameHostImpl* rfh) {
  if (!rfh->IsDOMContentLoaded())
    result->No(BackForwardCacheMetrics::NotRestoredReason::kLoading);

  // If the rfh has ever granted media access, prevent it from entering cache.
  // TODO(crbug.com/989379): Consider only blocking when there's an active
  //                         media stream.
  if (rfh->was_granted_media_access()) {
    result->No(
        BackForwardCacheMetrics::NotRestoredReason::kWasGrantedMediaAccess);
  }

  if (rfh->IsBackForwardCacheDisabled() && !ShouldIgnoreBlocklists()) {
    result->NoDueToDisableForRenderFrameHostCalled(
        rfh->back_forward_cache_disabled_reasons());
  }

  // Don't cache the page if it uses any disallowed features.
  // TODO(altimin): At the moment only the first detected failure is reported.
  // For reporting purposes it's a good idea to also collect this information
  // from children.
  if (uint64_t banned_features =
          GetDisallowedFeatures(rfh) & rfh->scheduler_tracked_features()) {
    if (!ShouldIgnoreBlocklists()) {
      result->NoDueToFeatures(banned_features);
    }
  }

  bool has_navigation_request = rfh->frame_tree_node()->navigation_request() ||
                                rfh->HasPendingCommitNavigation();
  // Do not cache if we have navigations in any of the subframes.
  if (rfh->GetParent() && has_navigation_request) {
    result->No(
        BackForwardCacheMetrics::NotRestoredReason::kSubframeIsNavigating);
  }

  for (size_t i = 0; i < rfh->child_count(); i++)
    CanStoreRenderFrameHost(result, rfh->child_at(i)->current_frame_host());
}

void BackForwardCacheImpl::StoreEntry(
    std::unique_ptr<BackForwardCacheImpl::Entry> entry) {
  TRACE_EVENT0("navigation", "BackForwardCache::StoreEntry");
  DCHECK(CanStoreDocument(entry->render_frame_host.get()));

#if defined(OS_ANDROID)
  if (!IsProcessBindingEnabled()) {
    // Set the priority of the main frame on entering the back-forward cache to
    // make sure the page gets evicted instead of foreground tab. This might not
    // become the effective priority of the process if it owns other higher
    // priority RenderWidgetHost. We don't need to reset the priority in
    // RestoreEntry as it is taken care by WebContentsImpl::NotifyFrameSwapped
    // on restoration.
    RenderWidgetHostImpl* rwh = entry->render_frame_host->GetRenderWidgetHost();
    ChildProcessImportance current_importance = rwh->importance();
    rwh->SetImportance(
        std::min(current_importance, kChildProcessImportanceParam.Get()));
  }
#endif

  entry->render_frame_host->EnterBackForwardCache();
  entries_.push_front(std::move(entry));

  size_t size_limit = cache_size_limit_for_testing_
                          ? cache_size_limit_for_testing_
                          : kBackForwardCacheLimit;
  // Evict the least recently used documents if the BackForwardCache list is
  // full.
  size_t available_count = 0;
  for (auto& stored_entry : entries_) {
    if (stored_entry->render_frame_host->is_evicted_from_back_forward_cache())
      continue;
    if (++available_count > size_limit) {
      stored_entry->render_frame_host->EvictFromBackForwardCacheWithReason(
          BackForwardCacheMetrics::NotRestoredReason::kCacheLimit);
    }
  }
}

std::unique_ptr<BackForwardCacheImpl::Entry> BackForwardCacheImpl::RestoreEntry(
    int navigation_entry_id) {
  TRACE_EVENT0("navigation", "BackForwardCache::RestoreEntry");
  // Select the RenderFrameHostImpl matching the navigation entry.
  auto matching_entry = std::find_if(
      entries_.begin(), entries_.end(),
      [navigation_entry_id](std::unique_ptr<Entry>& entry) {
        return entry->render_frame_host->nav_entry_id() == navigation_entry_id;
      });

  // Not found.
  if (matching_entry == entries_.end())
    return nullptr;

  // Don't restore an evicted frame.
  if ((*matching_entry)
          ->render_frame_host->is_evicted_from_back_forward_cache())
    return nullptr;

  std::unique_ptr<Entry> entry = std::move(*matching_entry);
  entries_.erase(matching_entry);
  entry->render_frame_host->LeaveBackForwardCache();

  RestoreBrowserControlsState(entry->render_frame_host.get());

  return entry;
}

void BackForwardCacheImpl::Flush() {
  TRACE_EVENT0("navigation", "BackForwardCache::Flush");
  for (std::unique_ptr<Entry>& entry : entries_) {
    entry->render_frame_host->EvictFromBackForwardCacheWithReason(
        BackForwardCacheMetrics::NotRestoredReason::kCacheFlushed);
  }
}

void BackForwardCacheImpl::Shutdown() {
  entries_.clear();
}

void BackForwardCacheImpl::EvictFramesInRelatedSiteInstances(
    SiteInstance* site_instance) {
  for (std::unique_ptr<Entry>& entry : entries_) {
    if (entry->render_frame_host->GetSiteInstance()->IsRelatedSiteInstance(
            site_instance)) {
      entry->render_frame_host->EvictFromBackForwardCacheWithReason(
          BackForwardCacheMetrics::NotRestoredReason::
              kConflictingBrowsingInstance);
    }
  }
}

void BackForwardCacheImpl::PostTaskToDestroyEvictedFrames() {
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&BackForwardCacheImpl::DestroyEvictedFrames,
                                weak_factory_.GetWeakPtr()));
}

// static
void BackForwardCache::DisableForRenderFrameHost(RenderFrameHost* rfh,
                                                 base::StringPiece reason) {
  DisableForRenderFrameHost(
      static_cast<RenderFrameHostImpl*>(rfh)->GetGlobalFrameRoutingId(),
      reason);
}

// static
void BackForwardCache::DisableForRenderFrameHost(GlobalFrameRoutingId id,
                                                 base::StringPiece reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (g_bfcache_disabled_test_observer)
    g_bfcache_disabled_test_observer->OnDisabledForFrameWithReason(id, reason);

  if (auto* rfh = RenderFrameHostImpl::FromID(id))
    rfh->DisableBackForwardCache(reason);
}

// static
bool BackForwardCache::EvictIfCached(GlobalFrameRoutingId id,
                                     base::StringPiece reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* rfh = RenderFrameHostImpl::FromID(id);
  if (rfh && rfh->is_in_back_forward_cache()) {
    BackForwardCacheCanStoreDocumentResult can_store;
    can_store.NoDueToDisableForRenderFrameHostCalled({reason.as_string()});
    rfh->EvictFromBackForwardCacheWithReasons(can_store);
    return true;
  }
  return false;
}

void BackForwardCacheImpl::DisableForTesting(DisableForTestingReason reason) {
  is_disabled_for_testing_ = true;

  // This could happen if a test populated some entries in the cache, then
  // called DisableForTesting(). This is not something we currently expect tests
  // to do.
  DCHECK(entries_.empty());
}

BackForwardCacheImpl::Entry* BackForwardCacheImpl::GetEntry(
    int navigation_entry_id) {
  auto matching_entry = std::find_if(
      entries_.begin(), entries_.end(),
      [navigation_entry_id](std::unique_ptr<Entry>& entry) {
        return entry->render_frame_host->nav_entry_id() == navigation_entry_id;
      });

  if (matching_entry == entries_.end())
    return nullptr;

  // Don't return the frame if it is evicted.
  if ((*matching_entry)
          ->render_frame_host->is_evicted_from_back_forward_cache())
    return nullptr;

  return (*matching_entry).get();
}

void BackForwardCacheImpl::DestroyEvictedFrames() {
  TRACE_EVENT0("navigation", "BackForwardCache::DestroyEvictedFrames");
  if (entries_.empty())
    return;
  entries_.erase(std::remove_if(
      entries_.begin(), entries_.end(), [](std::unique_ptr<Entry>& entry) {
        return entry->render_frame_host->is_evicted_from_back_forward_cache();
      }));
}

bool BackForwardCacheImpl::IsAllowed(const GURL& current_url) {
  // By convention, when |allowed_urls_| is empty, it means there are no
  // restrictions about what RenderFrameHost can enter the BackForwardCache.
  if (allowed_urls_.empty())
    return true;

  // Checking for each url in the |allowed_urls_|, if the current_url matches
  // the corresponding host and path is the prefix of the allowed url path. We
  // only check for host and path and not any other components including url
  // scheme here.
  const auto& entry = allowed_urls_.find(current_url.host());
  if (entry != allowed_urls_.end()) {
    for (auto allowed_path : entry->second) {
      if (current_url.path_piece().starts_with(allowed_path))
        return true;
    }
  }
  return false;
}
}  // namespace content
