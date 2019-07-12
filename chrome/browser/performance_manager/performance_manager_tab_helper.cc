// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/performance_manager_tab_helper.h"

#include <type_traits>
#include <vector>

#include "base/bind.h"
#include "base/stl_util.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/performance_manager.h"
#include "chrome/browser/performance_manager/render_process_user_data.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"

namespace performance_manager {

// static
PerformanceManagerTabHelper* PerformanceManagerTabHelper::first_ = nullptr;

// static
void PerformanceManagerTabHelper::DetachAndDestroyAll() {
  while (first_)
    first_->web_contents()->RemoveUserData(UserDataKey());
}

PerformanceManagerTabHelper::PerformanceManagerTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      performance_manager_(PerformanceManager::GetInstance()) {
  page_node_ = performance_manager_->CreatePageNode(
      WebContentsProxy(weak_factory_.GetWeakPtr()),
      web_contents->GetVisibility() == content::Visibility::VISIBLE,
      web_contents->IsCurrentlyAudible());

  // Dispatch creation notifications for any pre-existing frames.
  std::vector<content::RenderFrameHost*> existing_frames =
      web_contents->GetAllFrames();
  for (content::RenderFrameHost* frame : existing_frames) {
    // Only send notifications for created frames, the others will generate
    // creation notifications in due course (or not at all).
    if (frame->IsRenderFrameCreated())
      RenderFrameCreated(frame);
  }

  // Push this instance to the list.
  next_ = first_;
  if (next_)
    next_->prev_ = this;
  prev_ = nullptr;
  first_ = this;
}

PerformanceManagerTabHelper::~PerformanceManagerTabHelper() {
  // Ship our page and frame nodes to the PerformanceManager for incineration.
  std::vector<std::unique_ptr<NodeBase>> nodes;
  nodes.push_back(std::move(page_node_));
  for (auto& kv : frames_)
    nodes.push_back(std::move(kv.second));

  frames_.clear();

  // Delete the page and its entire frame tree from the graph.
  performance_manager_->BatchDeleteNodes(std::move(nodes));

  if (first_ == this)
    first_ = next_;

  if (prev_) {
    DCHECK_EQ(prev_->next_, this);
    prev_->next_ = next_;
  }
  if (next_) {
    DCHECK_EQ(next_->prev_, this);
    next_->prev_ = prev_;
  }
}

void PerformanceManagerTabHelper::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  DCHECK_NE(nullptr, render_frame_host);
  // This must not exist in the map yet.
  DCHECK(!base::Contains(frames_, render_frame_host));

  content::RenderFrameHost* parent = render_frame_host->GetParent();
  FrameNodeImpl* parent_frame_node = nullptr;
  if (parent) {
    DCHECK(base::Contains(frames_, parent));
    parent_frame_node = frames_[parent].get();
  }

  // Ideally this would strictly be a "Get", but it is possible in tests for
  // the the RenderProcessUserData to not have attached at this point.
  auto* process_node = RenderProcessUserData::GetOrCreateForRenderProcessHost(
                           render_frame_host->GetProcess())
                           ->process_node();

  auto* site_instance = render_frame_host->GetSiteInstance();

  // Create the frame node, and provide a callback that will run in the graph to
  // initialize it.
  std::unique_ptr<FrameNodeImpl> frame = performance_manager_->CreateFrameNode(
      process_node, page_node_.get(), parent_frame_node,
      render_frame_host->GetFrameTreeNodeId(),
      render_frame_host->GetDevToolsFrameToken(),
      site_instance->GetBrowsingInstanceId(), site_instance->GetId(),
      base::BindOnce(
          [](const GURL& url, bool is_current, FrameNodeImpl* frame_node) {
            if (!url.is_empty())
              frame_node->OnNavigationCommitted(url, /* same_document */ false);
            frame_node->SetIsCurrent(is_current);
          },
          render_frame_host->GetLastCommittedURL(),
          render_frame_host->IsCurrent()));

  frames_[render_frame_host] = std::move(frame);
}

void PerformanceManagerTabHelper::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  auto it = frames_.find(render_frame_host);
  if (it == frames_.end()) {
    // https://crbug.com/948088.
    // At the present time (May 2019), it's possible for speculative render
    // frame hosts to exist at the time this TabHelper is attached to a
    // WebContents. These speculative render frame hosts are not exposed in
    // enumeration, and so may be first observed at deletion time.
    return;
  }
  DCHECK(it != frames_.end());

  performance_manager_->DeleteNode(std::move(it->second));
  frames_.erase(it);
}

void PerformanceManagerTabHelper::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  // |old_host| is null when a new frame tree position is being created and a
  // new frame is its first occupant.
  FrameNodeImpl* old_frame = nullptr;
  if (old_host) {
    auto it = frames_.find(old_host);
    if (it != frames_.end()) {
      // This can be received for a frame that hasn't yet been created. We can
      // safely ignore this. It would be nice to track those frames too, but
      // since they're not yet "created" we'd have no guarantee of seeing a
      // corresponding delete and the frames can be leaked.
      old_frame = it->second.get();
    }
  }

  // It's entirely possible that this is the first time we're seeing this frame.
  // We'll eventually see a corresponding RenderFrameCreated if the frame ends
  // up actually being needed, so we can ignore it until that time. Artificially
  // creating the frame causes problems because we aren't guaranteed to see a
  // subsequent RenderFrameCreated call, meaning we won't see a
  // RenderFrameDeleted, and the frame node will be leaked until process tear
  // down.
  DCHECK(new_host);
  FrameNodeImpl* new_frame = nullptr;
  auto it = frames_.find(new_host);
  if (it != frames_.end()) {
    new_frame = it->second.get();
  } else if (new_host->IsRenderFrameCreated()) {
    // https://crbug.com/948088.
    // In the case of speculative frames already existent and created at attach
    // time, fake the creation event at this point.
    RenderFrameCreated(new_host);

    new_frame = frames_[new_host].get();
    DCHECK_NE(nullptr, new_frame);
  }
  // If neither frame could be looked up there's nothing to do.
  if (!old_frame && !new_frame)
    return;

  // Perform the swap in the graph.
  performance_manager_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](FrameNodeImpl* old_frame, FrameNodeImpl* new_frame) {
                       if (old_frame) {
                         DCHECK(old_frame->is_current());
                         old_frame->SetIsCurrent(false);
                       }
                       if (new_frame) {
                         DCHECK(!new_frame->is_current());
                         new_frame->SetIsCurrent(true);
                       }
                     },
                     base::Unretained(old_frame), base::Unretained(new_frame)));
}

void PerformanceManagerTabHelper::DidStartLoading() {
  PostToGraph(FROM_HERE, &PageNodeImpl::SetIsLoading, page_node_.get(), true);
}

void PerformanceManagerTabHelper::DidStopLoading() {
  PostToGraph(FROM_HERE, &PageNodeImpl::SetIsLoading, page_node_.get(), false);
}

void PerformanceManagerTabHelper::OnVisibilityChanged(
    content::Visibility visibility) {
  const bool is_visible = visibility == content::Visibility::VISIBLE;
  PostToGraph(FROM_HERE, &PageNodeImpl::SetIsVisible, page_node_.get(),
              is_visible);
}

void PerformanceManagerTabHelper::OnAudioStateChanged(bool audible) {
  PostToGraph(FROM_HERE, &PageNodeImpl::SetIsAudible, page_node_.get(),
              audible);
}

void PerformanceManagerTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted())
    return;

  // Grab the current time up front, as this is as close as we'll get to the
  // original commit time.
  base::TimeTicks navigation_committed_time = base::TimeTicks::Now();

  // Find the associated frame node.
  content::RenderFrameHost* render_frame_host =
      navigation_handle->GetRenderFrameHost();
  auto frame_it = frames_.find(render_frame_host);
  // TODO(siggi): Ideally this would be a DCHECK, but it seems it's possible
  //     to get a DidFinishNavigation notification for a deleted frame with
  //     the network service.
  if (frame_it == frames_.end())
    return;
  auto* frame_node = frame_it->second.get();

  // Notify the frame of the committed URL.
  GURL url = navigation_handle->GetURL();
  PostToGraph(FROM_HERE, &FrameNodeImpl::OnNavigationCommitted, frame_node, url,
              navigation_handle->IsSameDocument());

  if (navigation_handle->IsSameDocument() ||
      !navigation_handle->IsInMainFrame()) {
    return;
  }

  // Make sure the hierarchical structure is constructed before sending signal
  // to the performance manager.
  OnMainFrameNavigation(navigation_handle->GetNavigationId());
  PostToGraph(FROM_HERE, &PageNodeImpl::OnMainFrameNavigationCommitted,
              page_node_.get(), navigation_committed_time,
              navigation_handle->GetNavigationId(), url);
}

void PerformanceManagerTabHelper::TitleWasSet(content::NavigationEntry* entry) {
  // TODO(siggi): This logic belongs in the policy layer rather than here.
  if (!first_time_title_set_) {
    first_time_title_set_ = true;
    return;
  }
  PostToGraph(FROM_HERE, &PageNodeImpl::OnTitleUpdated, page_node_.get());
}

void PerformanceManagerTabHelper::DidUpdateFaviconURL(
    const std::vector<content::FaviconURL>& candidates) {
  // TODO(siggi): This logic belongs in the policy layer rather than here.
  if (!first_time_favicon_set_) {
    first_time_favicon_set_ = true;
    return;
  }
  PostToGraph(FROM_HERE, &PageNodeImpl::OnFaviconUpdated, page_node_.get());
}

void PerformanceManagerTabHelper::OnInterfaceRequestFromFrame(
    content::RenderFrameHost* render_frame_host,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  if (interface_name !=
      resource_coordinator::mojom::DocumentCoordinationUnit::Name_)
    return;

  auto it = frames_.find(render_frame_host);
  DCHECK(it != frames_.end());
  PostToGraph(FROM_HERE, &FrameNodeImpl::Bind, it->second.get(),
              resource_coordinator::mojom::DocumentCoordinationUnitRequest(
                  std::move(*interface_pipe)));
}

content::WebContents* PerformanceManagerTabHelper::GetWebContents() const {
  return web_contents();
}

int64_t PerformanceManagerTabHelper::LastNavigationId() const {
  return last_navigation_id_;
}

template <typename Functor, typename NodeType, typename... Args>
void PerformanceManagerTabHelper::PostToGraph(const base::Location& from_here,
                                              Functor&& functor,
                                              NodeType* node,
                                              Args&&... args) {
  static_assert(std::is_base_of<NodeBase, NodeType>::value,
                "NodeType must be descended from NodeBase");
  performance_manager_->task_runner()->PostTask(
      from_here, base::BindOnce(functor, base::Unretained(node),
                                std::forward<Args>(args)...));
}

void PerformanceManagerTabHelper::OnMainFrameNavigation(int64_t navigation_id) {
  last_navigation_id_ = navigation_id;
  ukm_source_id_ =
      ukm::ConvertToSourceId(navigation_id, ukm::SourceIdType::NAVIGATION_ID);
  PostToGraph(FROM_HERE, &PageNodeImpl::SetUkmSourceId, page_node_.get(),
              ukm_source_id_);

  first_time_title_set_ = false;
  first_time_favicon_set_ = false;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PerformanceManagerTabHelper)

}  // namespace performance_manager
