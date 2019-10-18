// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/back_forward_cache_impl.h"

#include <algorithm>
#include <string>

#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"
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

// Converts a WebSchedulerTrackedFeature to a bit for use in a bitmask.
constexpr uint64_t ToFeatureBit(WebSchedulerTrackedFeature feature) {
  return 1ull << static_cast<uint32_t>(feature);
}

void SetPageFrozenImpl(
    RenderFrameHostImpl* render_frame_host,
    bool frozen,
    std::unordered_set<RenderViewHostImpl*>* render_view_hosts) {
  RenderViewHostImpl* render_view_host = render_frame_host->render_view_host();
  // (Un)Freeze the frame's page if it is not (un)frozen yet.
  if (render_view_hosts->find(render_view_host) == render_view_hosts->end()) {
    // The state change for bfcache is:
    // PageHidden -> PageFrozen -> PageResumed -> PageShown.
    //
    // See: https://developers.google.com/web/updates/2018/07/page-lifecycle-api
    int rvh_routing_id = render_view_host->GetRoutingID();
    // TODO(dcheng): Page messages should be used in conjunction with
    // SendPageMessage(). Having it used to directly route a message to a
    // RenderView is somewhat unusual. Figure out why this is needed.
    if (frozen) {
      render_view_host->Send(
          new PageMsg_PutPageIntoBackForwardCache(rvh_routing_id));
    } else {
      render_view_host->Send(
          new PageMsg_RestorePageFromBackForwardCache(rvh_routing_id));
    }
    render_view_hosts->insert(render_view_host);
  }
  // Recurse on |render_frame_host|'s children.
  for (size_t index = 0; index < render_frame_host->child_count(); ++index) {
    RenderFrameHostImpl* child_frame_host =
        render_frame_host->child_at(index)->current_frame_host();
    SetPageFrozenImpl(child_frame_host, frozen, render_view_hosts);
  }
}

bool IsExtendedSupportEnabled() {
  static constexpr base::FeatureParam<bool> extended_support_enabled(
      &features::kBackForwardCache,
      "experimental extended supported feature set", false);
  return extended_support_enabled.Get();
}

uint64_t GetDisallowedFeatures(RenderFrameHostImpl* rfh) {
  // TODO(lowell): Finalize disallowed feature list, and test for each
  // disallowed feature.
  constexpr uint64_t kAlwaysDisallowedFeatures =
      ToFeatureBit(WebSchedulerTrackedFeature::kWebRTC) |
      ToFeatureBit(WebSchedulerTrackedFeature::kContainsPlugins) |
      ToFeatureBit(WebSchedulerTrackedFeature::kDedicatedWorkerOrWorklet) |
      ToFeatureBit(WebSchedulerTrackedFeature::kOutstandingNetworkRequest) |
      ToFeatureBit(
          WebSchedulerTrackedFeature::kOutstandingIndexedDBTransaction) |
      ToFeatureBit(
          WebSchedulerTrackedFeature::kHasScriptableFramesInMultipleTabs) |
      ToFeatureBit(
          WebSchedulerTrackedFeature::kRequestedNotificationsPermission) |
      ToFeatureBit(WebSchedulerTrackedFeature::kRequestedMIDIPermission) |
      ToFeatureBit(
          WebSchedulerTrackedFeature::kRequestedAudioCapturePermission) |
      ToFeatureBit(
          WebSchedulerTrackedFeature::kRequestedVideoCapturePermission) |
      ToFeatureBit(WebSchedulerTrackedFeature::kRequestedSensorsPermission) |
      ToFeatureBit(
          WebSchedulerTrackedFeature::kRequestedBackgroundWorkPermission) |
      ToFeatureBit(WebSchedulerTrackedFeature::kBroadcastChannel) |
      ToFeatureBit(WebSchedulerTrackedFeature::kIndexedDBConnection) |
      ToFeatureBit(WebSchedulerTrackedFeature::kWebGL) |
      ToFeatureBit(WebSchedulerTrackedFeature::kWebVR) |
      ToFeatureBit(WebSchedulerTrackedFeature::kWebXR) |
      ToFeatureBit(WebSchedulerTrackedFeature::kSharedWorker) |
      ToFeatureBit(WebSchedulerTrackedFeature::kWebXR) |
      ToFeatureBit(WebSchedulerTrackedFeature::kWebLocks);

  uint64_t result = kAlwaysDisallowedFeatures;

  if (!IsExtendedSupportEnabled()) {
    result |=
        ToFeatureBit(WebSchedulerTrackedFeature::kServiceWorkerControlledPage);
    result |= ToFeatureBit(
        WebSchedulerTrackedFeature::kRequestedGeolocationPermission);
  }

  // We do not cache documents which have cache-control: no-store header on
  // their main resource.
  if (!rfh->GetParent()) {
    result |= ToFeatureBit(
        WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoStore);
  }

  return result;
}

std::string DescribeFeatures(uint64_t blocklisted_features) {
  std::vector<std::string> features;
  for (size_t i = 0;
       i <= static_cast<size_t>(WebSchedulerTrackedFeature::kMaxValue); ++i) {
    if (blocklisted_features & (1 << i)) {
      features.push_back(blink::scheduler::FeatureToString(
          static_cast<WebSchedulerTrackedFeature>(i)));
    }
  }
  return base::JoinString(features, ", ");
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

}  // namespace

BackForwardCacheImpl::Entry::Entry(
    std::unique_ptr<RenderFrameHostImpl> rfh,
    RenderFrameProxyHostMap proxies,
    std::set<RenderViewHostImpl*> render_view_hosts)
    : render_frame_host(std::move(rfh)),
      proxy_hosts(std::move(proxies)),
      render_view_hosts(std::move(render_view_hosts)) {}
BackForwardCacheImpl::Entry::~Entry() {}

std::string BackForwardCacheImpl::CanStoreDocumentResult::ToString() {
  using Reason = BackForwardCacheMetrics::CanNotStoreDocumentReason;

  if (can_store)
    return "Yes";

  switch (reason.value()) {
    case Reason::kNotMainFrame:
      return "No: not a main frame";
    case Reason::kBackForwardCacheDisabled:
      return "No: BackForwardCache disabled";
    case Reason::kRelatedActiveContentsExist:
      return "No: related active contents exist";
    case Reason::kHTTPStatusNotOK:
      return "No: HTTP status is not OK";
    case Reason::kSchemeNotHTTPOrHTTPS:
      return "No: scheme is not HTTP or HTTPS";
    case Reason::kLoading:
      return "No: frame is not fully loaded";
    case Reason::kWasGrantedMediaAccess:
      return "No: frame was granted microphone or camera access";
    case Reason::kBlocklistedFeatures:
      return "No: blocklisted features: " +
             DescribeFeatures(blocklisted_features);
    case Reason::kDisableForRenderFrameHostCalled:
      return "No: BackForwardCache::DisableForRenderFrameHost() was called";
    case Reason::kDomainNotAllowed:
      return "No: This domain is not allowed to be stored in BackForwardCache";
    case Reason::kHTTPMethodNotGET:
      return "No: HTTP method is not GET";
    case Reason::kSubframeIsNavigating:
      return "No: subframe navigation is in progress";
  }
}

BackForwardCacheImpl::CanStoreDocumentResult::CanStoreDocumentResult(
    const CanStoreDocumentResult&) = default;
BackForwardCacheImpl::CanStoreDocumentResult::~CanStoreDocumentResult() =
    default;

BackForwardCacheImpl::CanStoreDocumentResult
BackForwardCacheImpl::CanStoreDocumentResult::Yes() {
  return CanStoreDocumentResult(true, base::nullopt, 0);
}

BackForwardCacheImpl::CanStoreDocumentResult
BackForwardCacheImpl::CanStoreDocumentResult::No(
    BackForwardCacheMetrics::CanNotStoreDocumentReason reason) {
  return CanStoreDocumentResult(false, reason, 0);
}

BackForwardCacheImpl::CanStoreDocumentResult
BackForwardCacheImpl::CanStoreDocumentResult::NoDueToFeatures(
    uint64_t blocklisted_features) {
  return CanStoreDocumentResult(
      false,
      BackForwardCacheMetrics::CanNotStoreDocumentReason::kBlocklistedFeatures,
      blocklisted_features);
}

BackForwardCacheImpl::CanStoreDocumentResult::CanStoreDocumentResult(
    bool can_store,
    base::Optional<BackForwardCacheMetrics::CanNotStoreDocumentReason> reason,
    uint64_t blocklisted_features)
    : can_store(can_store),
      reason(reason),
      blocklisted_features(blocklisted_features) {}

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

BackForwardCacheImpl::CanStoreDocumentResult
BackForwardCacheImpl::CanStoreDocument(RenderFrameHostImpl* rfh) {
  // Use the BackForwardCache only for the main frame.
  if (rfh->GetParent()) {
    return CanStoreDocumentResult::No(
        BackForwardCacheMetrics::CanNotStoreDocumentReason::kNotMainFrame);
  }

  if (!IsBackForwardCacheEnabled() || is_disabled_for_testing_) {
    return CanStoreDocumentResult::No(
        BackForwardCacheMetrics::CanNotStoreDocumentReason::
            kBackForwardCacheDisabled);
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
    return CanStoreDocumentResult::No(
        BackForwardCacheMetrics::CanNotStoreDocumentReason::
            kRelatedActiveContentsExist);
  }

  // Only store documents that have successful http status code.
  // Note that for error pages, |last_http_status_code| is equal to 0.
  if (rfh->last_http_status_code() != net::HTTP_OK) {
    return CanStoreDocumentResult::No(
        BackForwardCacheMetrics::CanNotStoreDocumentReason::kHTTPStatusNotOK);
  }

  // Only store documents that were fetched via HTTP GET method.
  if (rfh->last_http_method() != net::HttpRequestHeaders::kGetMethod) {
    return CanStoreDocumentResult::No(
        BackForwardCacheMetrics::CanNotStoreDocumentReason::kHTTPMethodNotGET);
  }

  // Do not store main document with non HTTP/HTTPS URL scheme. In particular,
  // this excludes the new tab page.
  if (!rfh->GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
    return CanStoreDocumentResult::No(
        BackForwardCacheMetrics::CanNotStoreDocumentReason::
            kSchemeNotHTTPOrHTTPS);
  }

  // Only store documents that have URLs allowed through experiment.
  if (!IsAllowed(rfh->GetLastCommittedURL())) {
    return CanStoreDocumentResult::No(
        BackForwardCacheMetrics::CanNotStoreDocumentReason::kDomainNotAllowed);
  }

  return CanStoreRenderFrameHost(rfh);
}

// Recursively checks whether this RenderFrameHost and all child frames
// can be cached.
BackForwardCacheImpl::CanStoreDocumentResult
BackForwardCacheImpl::CanStoreRenderFrameHost(RenderFrameHostImpl* rfh) {
  if (!rfh->dom_content_loaded())
    return CanStoreDocumentResult::No(
        BackForwardCacheMetrics::CanNotStoreDocumentReason::kLoading);

  // If the rfh has ever granted media access, prevent it from entering cache.
  // TODO(crbug.com/989379): Consider only blocking when there's an active
  //                         media stream.
  if (rfh->was_granted_media_access()) {
    return CanStoreDocumentResult::No(
        BackForwardCacheMetrics::CanNotStoreDocumentReason::
            kWasGrantedMediaAccess);
  }

  if (rfh->is_back_forward_cache_disallowed()) {
    return CanStoreDocumentResult::No(
        BackForwardCacheMetrics::CanNotStoreDocumentReason::
            kDisableForRenderFrameHostCalled);
  }

  // Don't cache the page if it uses any disallowed features.
  // TODO(altimin): At the moment only the first detected failure is reported.
  // For reporting purposes it's a good idea to also collect this information
  // from children.
  if (uint64_t banned_features =
          GetDisallowedFeatures(rfh) & rfh->scheduler_tracked_features()) {
    return CanStoreDocumentResult::NoDueToFeatures(banned_features);
  }

  bool has_navigation_request = rfh->frame_tree_node()->navigation_request() ||
                                rfh->HasPendingCommitNavigation();
  // Do not cache if we have navigations in any of the subframes.
  if (rfh->GetParent() && has_navigation_request) {
    return CanStoreDocumentResult::No(
        BackForwardCacheMetrics::CanNotStoreDocumentReason::
            kSubframeIsNavigating);
  }

  for (size_t i = 0; i < rfh->child_count(); i++) {
    CanStoreDocumentResult can_store_child =
        CanStoreRenderFrameHost(rfh->child_at(i)->current_frame_host());
    if (!can_store_child.can_store)
      return can_store_child;
  }

  return CanStoreDocumentResult::Yes();
}

void BackForwardCacheImpl::StoreEntry(
    std::unique_ptr<BackForwardCacheImpl::Entry> entry) {
  TRACE_EVENT0("navigation", "BackForwardCache::StoreEntry");
  DCHECK(CanStoreDocument(entry->render_frame_host.get()));

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
          BackForwardCacheMetrics::EvictedReason::kCacheLimit);
    }
  }
}

void BackForwardCacheImpl::Freeze(RenderFrameHostImpl* main_rfh) {
  // Several RenderFrameHost can live under the same RenderViewHost.
  // |frozen_render_view_hosts| keeps track of the ones that freezing has been
  // applied to.
  std::unordered_set<RenderViewHostImpl*> frozen_render_view_hosts;
  SetPageFrozenImpl(main_rfh, /*frozen = */ true, &frozen_render_view_hosts);
}

void BackForwardCacheImpl::Resume(RenderFrameHostImpl* main_rfh) {
  // |unfrozen_render_view_hosts| keeps track of the ones that resuming has
  // been applied to.
  std::unordered_set<RenderViewHostImpl*> unfrozen_render_view_hosts;
  SetPageFrozenImpl(main_rfh, /*frozen = */ false, &unfrozen_render_view_hosts);
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
  return entry;
}

void BackForwardCacheImpl::Flush() {
  TRACE_EVENT0("navigation", "BackForwardCache::Flush");
  entries_.clear();
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

  if (auto* rfh = RenderFrameHostImpl::FromID(id)) {
    rfh->DisallowBackForwardCache();

    RenderFrameHostImpl* frame = rfh;
    while (frame->GetParent())
      frame = frame->GetParent();

    if (BackForwardCacheMetrics* metrics = frame->GetBackForwardCacheMetrics())
      metrics->MarkDisableForRenderFrameHost(reason);
  }
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
