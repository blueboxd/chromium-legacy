// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/back_forward_cache.h"

#include <unordered_set>

#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/page_messages.h"
#include "content/public/common/navigation_policy.h"
#include "net/http/http_status_code.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"

namespace content {

namespace {

using blink::scheduler::WebSchedulerTrackedFeature;

// The number of document the BackForwardCache can hold per tab.
static constexpr size_t kBackForwardCacheLimit = 3;

// Converts a WebSchedulerTrackedFeature to a bit for use in a bitmask.
constexpr uint64_t ToFeatureBit(WebSchedulerTrackedFeature feature) {
  return 1 << static_cast<uint32_t>(feature);
}

// TODO(lowell): Finalize disallowed feature list, and test for each disallowed
// feature.
constexpr uint64_t kDisallowedFeatures =
    ToFeatureBit(WebSchedulerTrackedFeature::kWebRTC) |
    ToFeatureBit(WebSchedulerTrackedFeature::kContainsPlugins) |
    ToFeatureBit(WebSchedulerTrackedFeature::kDedicatedWorkerOrWorklet) |
    ToFeatureBit(WebSchedulerTrackedFeature::kServiceWorkerControlledPage) |
    ToFeatureBit(WebSchedulerTrackedFeature::kOutstandingIndexedDBTransaction) |
    ToFeatureBit(
        WebSchedulerTrackedFeature::kHasScriptableFramesInMultipleTabs) |
    ToFeatureBit(WebSchedulerTrackedFeature::kRequestedGeolocationPermission) |
    ToFeatureBit(
        WebSchedulerTrackedFeature::kRequestedNotificationsPermission) |
    ToFeatureBit(WebSchedulerTrackedFeature::kRequestedMIDIPermission) |
    ToFeatureBit(WebSchedulerTrackedFeature::kRequestedAudioCapturePermission) |
    ToFeatureBit(WebSchedulerTrackedFeature::kRequestedVideoCapturePermission) |
    ToFeatureBit(WebSchedulerTrackedFeature::kRequestedSensorsPermission) |
    ToFeatureBit(
        WebSchedulerTrackedFeature::kRequestedBackgroundWorkPermission) |
    ToFeatureBit(WebSchedulerTrackedFeature::kBroadcastChannel) |
    ToFeatureBit(WebSchedulerTrackedFeature::kIndexedDBConnection) |
    ToFeatureBit(WebSchedulerTrackedFeature::kWebGL) |
    ToFeatureBit(WebSchedulerTrackedFeature::kWebVR) |
    ToFeatureBit(WebSchedulerTrackedFeature::kWebXR);

void SetPageFrozenImpl(
    RenderFrameHostImpl* render_frame_host,
    bool freeze,
    std::unordered_set<RenderViewHostImpl*>* render_view_hosts) {
  RenderViewHostImpl* render_view_host = render_frame_host->render_view_host();
  // (Un)Freeze the frame's page if it is not (un)frozen yet.
  if (render_view_hosts->find(render_view_host) == render_view_hosts->end()) {
    // The state change for bfcache is:
    // PageHidden -> PageFrozen -> PageResumed -> PageShown.
    //
    // See: https://developers.google.com/web/updates/2018/07/page-lifecycle-api
    int rvh_routing_id = render_view_host->GetRoutingID();
    if (freeze) {
      // TODO(yuzus): Reconsider sending WasHidden here and investigate what
      // other browser vendors do.
      render_view_host->Send(new PageMsg_WasHidden(rvh_routing_id));
      render_view_host->Send(new PageMsg_SetPageFrozen(rvh_routing_id, true));
    } else {
      render_view_host->Send(new PageMsg_SetPageFrozen(rvh_routing_id, false));
      render_view_host->Send(new PageMsg_WasShown(rvh_routing_id));
    }
    render_view_hosts->insert(render_view_host);
  }
  // Recurse on |render_frame_host|'s children.
  for (size_t index = 0; index < render_frame_host->child_count(); ++index) {
    RenderFrameHostImpl* child_frame_host =
        render_frame_host->child_at(index)->current_frame_host();
    SetPageFrozenImpl(child_frame_host, freeze, render_view_hosts);
  }
}

}  // namespace

BackForwardCache::BackForwardCache() = default;
BackForwardCache::~BackForwardCache() = default;

bool BackForwardCache::CanStoreDocument(RenderFrameHostImpl* rfh) {
  // Use the BackForwardCache only for the main frame.
  if (rfh->GetParent())
    return false;

  if (!IsBackForwardCacheEnabled() || is_disabled_for_testing_)
    return false;

  // If the rfh has ever granted media access, prevent it from entering cache.
  // TODO(crbug.com/989379): Consider only blocking when there's an active
  //                         media stream.
  // TODO(crbug.com/989379): Consider also checking whether any subframes
  //                         have requested an media stream.
  if (rfh->was_granted_media_access())
    return false;

  // Note that we check is_loading on the rfh directly, rather than calling
  // FrameTreeNode::IsLoading. This is because FrameTreeNode is already
  // loading the new page being navigated to.
  // TODO(lowell): Consider also checking whether any subframes are loading.
  if (rfh->is_loading())
    return false;

  // Don't enable BackForwardCache if the page has any disallowed features.
  // TODO(lowell): Handle races involving scheduler_tracked_features.
  // One solution could be to listen for changes to scheduler_tracked_features
  // and if we see a frame in bfcache starting to use something forbidden, evict
  // it from the bfcache.
  if (kDisallowedFeatures & rfh->scheduler_tracked_features())
    return false;

  // Two pages in the same BrowsingInstance can script each other. When a page
  // can be scripted from outside, it can't enter the BackForwardCache.
  //
  // The "RelatedActiveContentsCount" below is compared against 0, not 1. This
  // is because the |rfh| is not a "current" RenderFrameHost anymore. It is not
  // "active" itself.
  //
  // This check makes sure the old and new document aren't sharing the same
  // BrowsingInstance.
  if (rfh->GetSiteInstance()->GetRelatedActiveContentsCount() != 0)
    return false;

  // Only store documents that have successful http status code.
  // Note that for non-http navigations, |last_http_status_code| is equal to 0.
  if (rfh->last_http_status_code() != net::HTTP_OK)
    return false;

  return true;
}

void BackForwardCache::StoreDocument(std::unique_ptr<RenderFrameHostImpl> rfh) {
  DCHECK(CanStoreDocument(rfh.get()));

  rfh->EnterBackForwardCache();
  render_frame_hosts_.push_front(std::move(rfh));

  // Remove the last recently used document if the BackForwardCache list is
  // full.
  if (render_frame_hosts_.size() > kBackForwardCacheLimit) {
    // TODO(arthursonzogni): Handle RenderFrame deletion appropriately.
    render_frame_hosts_.pop_back();
  }
}

void BackForwardCache::Freeze(RenderFrameHostImpl* main_rfh) {
  // Several RenderFrameHost can live under the same RenderViewHost.
  // |frozen_render_view_hosts| keeps track of the ones that freezing has been
  // applied to.
  std::unordered_set<RenderViewHostImpl*> frozen_render_view_hosts;
  SetPageFrozenImpl(main_rfh, /*freeze = */ true, &frozen_render_view_hosts);
}

void BackForwardCache::Resume(RenderFrameHostImpl* main_rfh) {
  // |unfrozen_render_view_hosts| keeps track of the ones that resuming has
  // been applied to.
  std::unordered_set<RenderViewHostImpl*> unfrozen_render_view_hosts;
  SetPageFrozenImpl(main_rfh, /*freeze = */ false, &unfrozen_render_view_hosts);
}

void BackForwardCache::EvictDocument(RenderFrameHostImpl* render_frame_host) {
  auto matching_rfh = std::find_if(
      render_frame_hosts_.begin(), render_frame_hosts_.end(),
      [render_frame_host](std::unique_ptr<RenderFrameHostImpl>& rfh) {
        return rfh.get() == render_frame_host;
      });

  DCHECK(matching_rfh != render_frame_hosts_.end());
  render_frame_hosts_.erase(matching_rfh);
}

std::unique_ptr<RenderFrameHostImpl> BackForwardCache::RestoreDocument(
    int navigation_entry_id) {
  // Select the RenderFrameHostImpl matching the navigation entry.
  auto matching_rfh = std::find_if(
      render_frame_hosts_.begin(), render_frame_hosts_.end(),
      [navigation_entry_id](std::unique_ptr<RenderFrameHostImpl>& rfh) {
        return rfh->nav_entry_id() == navigation_entry_id;
      });

  // Not found.
  if (matching_rfh == render_frame_hosts_.end())
    return nullptr;

  std::unique_ptr<RenderFrameHostImpl> rfh = std::move(*matching_rfh);
  render_frame_hosts_.erase(matching_rfh);
  rfh->LeaveBackForwardCache();
  return rfh;
}

void BackForwardCache::Flush() {
  render_frame_hosts_.clear();
}

void BackForwardCache::DisableForTesting() {
  is_disabled_for_testing_ = true;

  // This could happen if a test populated some pages in the cache, then
  // called DisableForTesting(). This is not something we currently expect tests
  // to do.
  DCHECK(render_frame_hosts_.empty());
}

bool BackForwardCache::ContainsDocument(int navigation_entry_id) {
  auto matching_rfh = std::find_if(
      render_frame_hosts_.begin(), render_frame_hosts_.end(),
      [navigation_entry_id](std::unique_ptr<RenderFrameHostImpl>& rfh) {
        return rfh->nav_entry_id() == navigation_entry_id;
      });

  return matching_rfh != render_frame_hosts_.end();
}

}  // namespace content
