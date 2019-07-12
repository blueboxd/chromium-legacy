// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/webui_graph_dump_impl.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/graph_impl.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/graph/system_node_impl.h"
#include "chrome/browser/performance_manager/public/web_contents_proxy.h"
#include "chrome/browser/profiles/profile.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_callback.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace performance_manager {

class WebUIGraphDumpImpl::FaviconRequestHelper {
 public:
  FaviconRequestHelper(base::WeakPtr<WebUIGraphDumpImpl> graph_dump,
                       scoped_refptr<base::SequencedTaskRunner> task_runner);

  void RequestFavicon(GURL page_url,
                      WebContentsProxy contents_proxy,
                      int64_t serialization_id);
  void FaviconDataAvailable(int64_t serialization_id,
                            const favicon_base::FaviconRawBitmapResult& result);

 private:
  std::unique_ptr<base::CancelableTaskTracker> cancelable_task_tracker_;

  base::WeakPtr<WebUIGraphDumpImpl> graph_dump_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(FaviconRequestHelper);
};

WebUIGraphDumpImpl::FaviconRequestHelper::FaviconRequestHelper(
    base::WeakPtr<WebUIGraphDumpImpl> graph_dump,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : graph_dump_(graph_dump), task_runner_(task_runner) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void WebUIGraphDumpImpl::FaviconRequestHelper::RequestFavicon(
    GURL page_url,
    WebContentsProxy contents_proxy,
    int64_t serialization_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  content::WebContents* web_contents = contents_proxy.Get();
  if (!web_contents)
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile)
    return;

  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (!favicon_service)
    return;

  if (!cancelable_task_tracker_)
    cancelable_task_tracker_ = std::make_unique<base::CancelableTaskTracker>();

  constexpr size_t kIconSize = 16;
  constexpr bool kFallbackToHost = true;
  // It's safe to pass this unretained here, as the tasks are cancelled
  // on deletion of the cancelable task tracker.
  favicon_service->GetRawFaviconForPageURL(
      page_url, {favicon_base::IconType::kFavicon}, kIconSize, kFallbackToHost,
      base::BindRepeating(&FaviconRequestHelper::FaviconDataAvailable,
                          base::Unretained(this), serialization_id),
      cancelable_task_tracker_.get());
}

void WebUIGraphDumpImpl::FaviconRequestHelper::FaviconDataAvailable(
    int64_t serialization_id,
    const favicon_base::FaviconRawBitmapResult& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result.is_valid())
    return;

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebUIGraphDumpImpl::SendFaviconNotification, graph_dump_,
                     serialization_id, result.bitmap_data));
}

WebUIGraphDumpImpl::WebUIGraphDumpImpl(GraphImpl* graph)
    : graph_(graph), binding_(this) {
  DCHECK(graph);
}

WebUIGraphDumpImpl::~WebUIGraphDumpImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (change_subscriber_) {
    graph_->UnregisterObserver(this);
    for (auto* node : graph_->nodes())
      node->RemoveObserver(this);
  }

  // The favicon helper must be deleted on the UI thread.
  if (favicon_request_helper_) {
    content::BrowserThread::DeleteSoon(content::BrowserThread::UI, FROM_HERE,
                                       std::move(favicon_request_helper_));
  }
}

void WebUIGraphDumpImpl::Bind(mojom::WebUIGraphDumpRequest request,
                              base::OnceClosure error_handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  binding_.Bind(std::move(request));
  binding_.set_connection_error_handler(std::move(error_handler));
}

namespace {

template <typename FunctionType>
void ForFrameAndOffspring(FrameNodeImpl* parent_frame, FunctionType on_frame) {
  on_frame(parent_frame);

  for (FrameNodeImpl* child_frame : parent_frame->child_frame_nodes())
    ForFrameAndOffspring(child_frame, on_frame);
}

}  // namespace

void WebUIGraphDumpImpl::SubscribeToChanges(
    mojom::WebUIGraphChangeStreamPtr change_subscriber) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  change_subscriber_ = std::move(change_subscriber);

  // Send creation notifications for all existing nodes and subscribe to them.
  for (ProcessNodeImpl* process_node : graph_->GetAllProcessNodeImpls()) {
    SendProcessNotification(process_node, true);
    process_node->AddObserver(this);
  }
  for (PageNodeImpl* page_node : graph_->GetAllPageNodeImpls()) {
    SendPageNotification(page_node, true);
    StartPageFaviconRequest(page_node);
    page_node->AddObserver(this);

    // Dispatch preorder frame notifications.
    for (FrameNodeImpl* main_frame_node : page_node->main_frame_nodes()) {
      ForFrameAndOffspring(main_frame_node, [this](FrameNodeImpl* frame_node) {
        frame_node->AddObserver(this);
        this->SendFrameNotification(frame_node, true);
        this->StartFrameFaviconRequest(frame_node);
      });
    }
  }

  // Subscribe to future changes to the graph.
  graph_->RegisterObserver(this);
}

bool WebUIGraphDumpImpl::ShouldObserve(const NodeBase* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

void WebUIGraphDumpImpl::OnNodeAdded(NodeBase* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (node->type()) {
    case FrameNodeImpl::Type(): {
      FrameNodeImpl* frame_node = FrameNodeImpl::FromNodeBase(node);
      SendFrameNotification(frame_node, true);
      StartFrameFaviconRequest(frame_node);
    } break;
    case PageNodeImpl::Type(): {
      PageNodeImpl* page_node = PageNodeImpl::FromNodeBase(node);
      SendPageNotification(page_node, true);
      StartPageFaviconRequest(page_node);
    } break;
    case ProcessNodeImpl::Type():
      SendProcessNotification(ProcessNodeImpl::FromNodeBase(node), true);
      break;
    case SystemNodeImpl::Type():
      break;
    case NodeTypeEnum::kInvalidType:
      break;
  }
}

void WebUIGraphDumpImpl::OnBeforeNodeRemoved(NodeBase* node) {
  SendDeletionNotification(node);
}

void WebUIGraphDumpImpl::OnIsCurrentChanged(FrameNodeImpl* frame_node) {
  SendFrameNotification(frame_node, false);
}

void WebUIGraphDumpImpl::OnNetworkAlmostIdleChanged(FrameNodeImpl* frame_node) {
  SendFrameNotification(frame_node, false);
}

void WebUIGraphDumpImpl::OnLifecycleStateChanged(FrameNodeImpl* frame_node) {
  SendFrameNotification(frame_node, false);
}

void WebUIGraphDumpImpl::OnURLChanged(FrameNodeImpl* frame_node) {
  SendFrameNotification(frame_node, false);
  StartFrameFaviconRequest(frame_node);
}

void WebUIGraphDumpImpl::OnIsVisibleChanged(PageNodeImpl* page_node) {
  SendPageNotification(page_node, false);
}

void WebUIGraphDumpImpl::OnIsAudibleChanged(PageNodeImpl* page_node) {
  SendPageNotification(page_node, false);
}

void WebUIGraphDumpImpl::OnIsLoadingChanged(PageNodeImpl* page_node) {
  SendPageNotification(page_node, false);
}

void WebUIGraphDumpImpl::OnUkmSourceIdChanged(PageNodeImpl* page_node) {
  SendPageNotification(page_node, false);
}

void WebUIGraphDumpImpl::OnLifecycleStateChanged(PageNodeImpl* page_node) {
  SendPageNotification(page_node, false);
}

void WebUIGraphDumpImpl::OnPageAlmostIdleChanged(PageNodeImpl* page_node) {
  SendPageNotification(page_node, false);
}

void WebUIGraphDumpImpl::OnFaviconUpdated(PageNodeImpl* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  StartPageFaviconRequest(page_node);
}

void WebUIGraphDumpImpl::OnMainFrameNavigationCommitted(
    PageNodeImpl* page_node) {
  SendPageNotification(page_node, false);
  StartPageFaviconRequest(page_node);
}

void WebUIGraphDumpImpl::OnExpectedTaskQueueingDurationSample(
    ProcessNodeImpl* process_node) {
  SendProcessNotification(process_node, false);
}

void WebUIGraphDumpImpl::OnMainThreadTaskLoadIsLow(
    ProcessNodeImpl* process_node) {
  SendProcessNotification(process_node, false);
}

void WebUIGraphDumpImpl::SetGraph(GraphImpl* graph) {
  DCHECK(!graph || graph_ == graph);
}

WebUIGraphDumpImpl::FaviconRequestHelper*
WebUIGraphDumpImpl::EnsureFaviconRequestHelper() {
  if (!favicon_request_helper_) {
    favicon_request_helper_ = std::make_unique<FaviconRequestHelper>(
        weak_factory_.GetWeakPtr(), base::SequencedTaskRunnerHandle::Get());
  }

  return favicon_request_helper_.get();
}

void WebUIGraphDumpImpl::StartPageFaviconRequest(PageNodeImpl* page_node) {
  if (!page_node->main_frame_url().is_valid())
    return;

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&FaviconRequestHelper::RequestFavicon,
                     base::Unretained(EnsureFaviconRequestHelper()),
                     page_node->main_frame_url(), page_node->contents_proxy(),
                     NodeBase::GetSerializationId(page_node)));
}

void WebUIGraphDumpImpl::StartFrameFaviconRequest(FrameNodeImpl* frame_node) {
  if (!frame_node->url().is_valid())
    return;

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&FaviconRequestHelper::RequestFavicon,
                     base::Unretained(EnsureFaviconRequestHelper()),
                     frame_node->url(),
                     frame_node->page_node()->contents_proxy(),
                     NodeBase::GetSerializationId(frame_node)));
}

void WebUIGraphDumpImpl::SendFrameNotification(FrameNodeImpl* frame,
                                               bool created) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(https://crbug.com/961785): Add more frame properties.
  mojom::WebUIFrameInfoPtr frame_info = mojom::WebUIFrameInfo::New();

  frame_info->id = NodeBase::GetSerializationId(frame);

  auto* parent_frame = frame->parent_frame_node();
  frame_info->parent_frame_id = NodeBase::GetSerializationId(parent_frame);

  auto* process = frame->process_node();
  frame_info->process_id = NodeBase::GetSerializationId(process);

  auto* page = frame->page_node();
  frame_info->page_id = NodeBase::GetSerializationId(page);

  frame_info->url = frame->url();

  if (created)
    change_subscriber_->FrameCreated(std::move(frame_info));
  else
    change_subscriber_->FrameChanged(std::move(frame_info));
}

void WebUIGraphDumpImpl::SendPageNotification(PageNodeImpl* page_node,
                                              bool created) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(https://crbug.com/961785): Add more page_node properties.
  mojom::WebUIPageInfoPtr page_info = mojom::WebUIPageInfo::New();

  page_info->id = NodeBase::GetSerializationId(page_node);
  page_info->main_frame_url = page_node->main_frame_url();
  if (created)
    change_subscriber_->PageCreated(std::move(page_info));
  else
    change_subscriber_->PageChanged(std::move(page_info));
}

void WebUIGraphDumpImpl::SendProcessNotification(ProcessNodeImpl* process,
                                                 bool created) {
  // TODO(https://crbug.com/961785): Add more process properties.
  mojom::WebUIProcessInfoPtr process_info = mojom::WebUIProcessInfo::New();

  process_info->id = NodeBase::GetSerializationId(process);
  process_info->pid = process->process_id();
  process_info->cumulative_cpu_usage = process->cumulative_cpu_usage();
  process_info->private_footprint_kb = process->private_footprint_kb();

  if (created)
    change_subscriber_->ProcessCreated(std::move(process_info));
  else
    change_subscriber_->ProcessChanged(std::move(process_info));
}

void WebUIGraphDumpImpl::SendDeletionNotification(NodeBase* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  change_subscriber_->NodeDeleted(NodeBase::GetSerializationId(node));
}

void WebUIGraphDumpImpl::SendFaviconNotification(
    int64_t serialization_id,
    scoped_refptr<base::RefCountedMemory> bitmap_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(0u, bitmap_data->size());

  mojom::WebUIFavIconInfoPtr icon_info = mojom::WebUIFavIconInfo::New();
  icon_info->node_id = serialization_id;

  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(bitmap_data->front()),
                        bitmap_data->size()),
      &icon_info->icon_data);

  change_subscriber_->FavIconDataAvailable(std::move(icon_info));
}

}  // namespace performance_manager
