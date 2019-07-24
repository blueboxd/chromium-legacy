// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_api_frame_id_map.h"

#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_host.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/constants.h"

namespace extensions {

namespace {

// The map is accessed on the IO and UI thread, so construct it once and never
// delete it.
base::LazyInstance<ExtensionApiFrameIdMap>::Leaky g_map_instance =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

const int ExtensionApiFrameIdMap::kInvalidFrameId = -1;
const int ExtensionApiFrameIdMap::kTopFrameId = 0;

ExtensionApiFrameIdMap::FrameData::FrameData()
    : frame_id(kInvalidFrameId),
      parent_frame_id(kInvalidFrameId),
      tab_id(extension_misc::kUnknownTabId),
      window_id(extension_misc::kUnknownWindowId) {}

ExtensionApiFrameIdMap::FrameData::FrameData(int frame_id,
                                             int parent_frame_id,
                                             int tab_id,
                                             int window_id,
                                             GURL last_committed_main_frame_url)
    : frame_id(frame_id),
      parent_frame_id(parent_frame_id),
      tab_id(tab_id),
      window_id(window_id),
      last_committed_main_frame_url(std::move(last_committed_main_frame_url)) {}

ExtensionApiFrameIdMap::FrameData::~FrameData() = default;

ExtensionApiFrameIdMap::FrameData::FrameData(
    const ExtensionApiFrameIdMap::FrameData& other) = default;
ExtensionApiFrameIdMap::FrameData& ExtensionApiFrameIdMap::FrameData::operator=(
    const ExtensionApiFrameIdMap::FrameData& other) = default;

ExtensionApiFrameIdMap::RenderFrameIdKey::RenderFrameIdKey()
    : render_process_id(content::ChildProcessHost::kInvalidUniqueID),
      frame_routing_id(MSG_ROUTING_NONE) {}

ExtensionApiFrameIdMap::RenderFrameIdKey::RenderFrameIdKey(
    int render_process_id,
    int frame_routing_id)
    : render_process_id(render_process_id),
      frame_routing_id(frame_routing_id) {}

ExtensionApiFrameIdMap::FrameDataCallbacks::FrameDataCallbacks()
    : is_iterating(false) {}

ExtensionApiFrameIdMap::FrameDataCallbacks::FrameDataCallbacks(
    const FrameDataCallbacks& other) = default;

ExtensionApiFrameIdMap::FrameDataCallbacks::~FrameDataCallbacks() {}

bool ExtensionApiFrameIdMap::RenderFrameIdKey::operator<(
    const RenderFrameIdKey& other) const {
  return std::tie(render_process_id, frame_routing_id) <
         std::tie(other.render_process_id, other.frame_routing_id);
}

bool ExtensionApiFrameIdMap::RenderFrameIdKey::operator==(
    const RenderFrameIdKey& other) const {
  return render_process_id == other.render_process_id &&
         frame_routing_id == other.frame_routing_id;
}

ExtensionApiFrameIdMap::ExtensionApiFrameIdMap() {
  // The browser client can be null in unittests.
  if (ExtensionsBrowserClient::Get()) {
    helper_ =
        ExtensionsBrowserClient::Get()->CreateExtensionApiFrameIdMapHelper(
            this);
  }
}

ExtensionApiFrameIdMap::~ExtensionApiFrameIdMap() {}

// static
ExtensionApiFrameIdMap* ExtensionApiFrameIdMap::Get() {
  return g_map_instance.Pointer();
}

// static
int ExtensionApiFrameIdMap::GetFrameId(content::RenderFrameHost* rfh) {
  if (!rfh)
    return kInvalidFrameId;
  if (rfh->GetParent())
    return rfh->GetFrameTreeNodeId();
  return kTopFrameId;
}

// static
int ExtensionApiFrameIdMap::GetFrameId(
    content::NavigationHandle* navigation_handle) {
  return navigation_handle->IsInMainFrame()
             ? kTopFrameId
             : navigation_handle->GetFrameTreeNodeId();
}

// static
int ExtensionApiFrameIdMap::GetParentFrameId(content::RenderFrameHost* rfh) {
  return rfh ? GetFrameId(rfh->GetParent()) : kInvalidFrameId;
}

// static
int ExtensionApiFrameIdMap::GetParentFrameId(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame())
    return kInvalidFrameId;

  if (navigation_handle->IsParentMainFrame())
    return kTopFrameId;

  return navigation_handle->GetParentFrame()->GetFrameTreeNodeId();
}

// static
content::RenderFrameHost* ExtensionApiFrameIdMap::GetRenderFrameHostById(
    content::WebContents* web_contents,
    int frame_id) {
  // Although it is technically possible to map |frame_id| to a RenderFrameHost
  // without WebContents, we choose to not do that because in the extension API
  // frameIds are only guaranteed to be meaningful in combination with a tabId.
  if (!web_contents)
    return nullptr;

  if (frame_id == kInvalidFrameId)
    return nullptr;

  if (frame_id == kTopFrameId)
    return web_contents->GetMainFrame();

  DCHECK_GE(frame_id, 1);

  // Unfortunately, extension APIs do not know which process to expect for a
  // given frame ID, so we must use an unsafe API here that could return a
  // different RenderFrameHost than the caller may have expected (e.g., one that
  // changed after a cross-process navigation).
  return web_contents->UnsafeFindFrameByFrameTreeNodeId(frame_id);
}

ExtensionApiFrameIdMap::FrameData ExtensionApiFrameIdMap::KeyToValue(
    const RenderFrameIdKey& key) const {
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      key.render_process_id, key.frame_routing_id);

  if (!rfh || !rfh->IsRenderFrameLive())
    return FrameData();

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);

  // The RenderFrameHost may not have an associated WebContents in cases
  // such as interstitial pages.
  GURL last_committed_main_frame_url =
      web_contents ? web_contents->GetLastCommittedURL() : GURL();
  int tab_id = extension_misc::kUnknownTabId;
  int window_id = extension_misc::kUnknownWindowId;
  if (helper_)
    helper_->PopulateTabData(rfh, &tab_id, &window_id);
  return FrameData(GetFrameId(rfh), GetParentFrameId(rfh), tab_id, window_id,
                   std::move(last_committed_main_frame_url));
}

ExtensionApiFrameIdMap::FrameData ExtensionApiFrameIdMap::LookupFrameDataOnUI(
    const RenderFrameIdKey& key,
    bool check_deleted_frames) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  FrameDataMap::const_iterator frame_id_iter = frame_data_map_.find(key);

  if (frame_id_iter != frame_data_map_.end())
    return frame_id_iter->second;

  if (check_deleted_frames) {
    frame_id_iter = deleted_frame_data_map_.find(key);
    if (frame_id_iter != deleted_frame_data_map_.end())
      return frame_id_iter->second;
  }

  FrameData data = KeyToValue(key);
  // Don't save invalid values in the map.
  if (data.frame_id != kInvalidFrameId)
    frame_data_map_.insert({key, data});

  return data;
}

ExtensionApiFrameIdMap::FrameData ExtensionApiFrameIdMap::GetFrameData(
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const RenderFrameIdKey key(render_process_id, render_frame_id);
  return LookupFrameDataOnUI(key, true /* check_deleted_frames */);
}

void ExtensionApiFrameIdMap::InitializeRenderFrameData(
    content::RenderFrameHost* rfh) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(rfh);
  DCHECK(rfh->IsRenderFrameLive());

  const RenderFrameIdKey key(rfh->GetProcess()->GetID(), rfh->GetRoutingID());
  LookupFrameDataOnUI(key, false /* check_deleted_frames */);
  DCHECK(frame_data_map_.find(key) != frame_data_map_.end());
}

void ExtensionApiFrameIdMap::OnRenderFrameDeleted(
    content::RenderFrameHost* rfh) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(rfh);

  const RenderFrameIdKey key(rfh->GetProcess()->GetID(), rfh->GetRoutingID());
  // TODO(http://crbug.com/522129): This is necessary right now because beacon
  // requests made in window.onunload may start after this has been called.
  // Delay the RemoveFrameData() call, so we will still have the frame data
  // cached when the beacon request comes in.
  auto iter = frame_data_map_.find(key);
  if (iter == frame_data_map_.end())
    return;

  deleted_frame_data_map_.insert({key, iter->second});
  frame_data_map_.erase(key);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](ExtensionApiFrameIdMap* self, const RenderFrameIdKey& key) {
            self->deleted_frame_data_map_.erase(key);
          },
          base::Unretained(this), key));
}

void ExtensionApiFrameIdMap::UpdateTabAndWindowId(
    int tab_id,
    int window_id,
    content::RenderFrameHost* rfh) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(rfh);
  const RenderFrameIdKey key(rfh->GetProcess()->GetID(), rfh->GetRoutingID());

  // Only track FrameData for live render frames.
  if (!rfh->IsRenderFrameLive()) {
    return;
  }

  auto iter = frame_data_map_.find(key);
  // The FrameData for |rfh| should have already been initialized.
  DCHECK(iter != frame_data_map_.end());
  iter->second.tab_id = tab_id;
  iter->second.window_id = window_id;
}

void ExtensionApiFrameIdMap::OnMainFrameReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(navigation_handle->IsInMainFrame());

  bool did_insert = false;
  std::tie(std::ignore, did_insert) =
      ready_to_commit_document_navigations_.insert(navigation_handle);
  DCHECK(did_insert);

  content::RenderFrameHost* main_frame =
      navigation_handle->GetRenderFrameHost();
  DCHECK(main_frame);

  // We only track live frames.
  if (!main_frame->IsRenderFrameLive())
    return;

  const RenderFrameIdKey key(main_frame->GetProcess()->GetID(),
                             main_frame->GetRoutingID());
  auto iter = frame_data_map_.find(key);

  // We must have already cached the FrameData for this in
  // InitializeRenderFrameHost.
  DCHECK(iter != frame_data_map_.end());
  iter->second.pending_main_frame_url = navigation_handle->GetURL();
}

void ExtensionApiFrameIdMap::OnMainFrameDidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(navigation_handle->IsInMainFrame());

  bool did_fire_ready_to_commit_navigation =
      !!ready_to_commit_document_navigations_.erase(navigation_handle);

  // It's safe to call NavigationHandle::GetRenderFrameHost here iff the
  // navigation committed or a ReadyToCommitNavigation event was dispatched for
  // this navigation.
  // Note a RenderFrameHost might not be associated with the NavigationHandle in
  // WebContentsObserver::DidFinishNavigation. This might happen when the
  // navigation doesn't commit which might happen for a variety of reasons like
  // the network network request to fetch the navigation url failed, the
  // navigation was cancelled, by say a NavigationThrottle etc.
  // There's nothing to do if the RenderFrameHost can't be fetched for this
  // navigation.
  bool can_fetch_render_frame_host =
      navigation_handle->HasCommitted() || did_fire_ready_to_commit_navigation;
  if (!can_fetch_render_frame_host)
    return;

  content::RenderFrameHost* main_frame =
      navigation_handle->GetRenderFrameHost();
  DCHECK(main_frame);

  // We only track live frames.
  if (!main_frame->IsRenderFrameLive())
    return;

  const RenderFrameIdKey key(main_frame->GetProcess()->GetID(),
                             main_frame->GetRoutingID());
  auto iter = frame_data_map_.find(key);

  // We must have already cached the FrameData for this in
  // InitializeRenderFrameHost.
  DCHECK(iter != frame_data_map_.end());
  iter->second.last_committed_main_frame_url =
      main_frame->GetLastCommittedURL();
  iter->second.pending_main_frame_url = base::nullopt;
}

bool ExtensionApiFrameIdMap::HasCachedFrameDataForTesting(
    content::RenderFrameHost* rfh) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!rfh)
    return false;

  const RenderFrameIdKey key(rfh->GetProcess()->GetID(), rfh->GetRoutingID());
  return frame_data_map_.find(key) != frame_data_map_.end();
}

size_t ExtensionApiFrameIdMap::GetFrameDataCountForTesting() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return frame_data_map_.size();
}

}  // namespace extensions
