// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/discards/webui_graph_dump_impl.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/performance_manager/public/graph/graph.h"
#include "chrome/browser/performance_manager/public/performance_manager.h"
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

WebUIGraphDumpImpl::WebUIGraphDumpImpl() : binding_(this) {}

WebUIGraphDumpImpl::~WebUIGraphDumpImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!graph_);
  DCHECK(!change_subscriber_);
  DCHECK(!favicon_request_helper_);
}

// static
void WebUIGraphDumpImpl::CreateAndBind(mojom::WebUIGraphDumpRequest request,
                                       Graph* graph) {
  std::unique_ptr<WebUIGraphDumpImpl> dump =
      std::make_unique<WebUIGraphDumpImpl>();

  dump->BindWithGraph(graph, std::move(request));
  graph->PassToGraph(std::move(dump));
}

void WebUIGraphDumpImpl::BindWithGraph(Graph* graph,
                                       mojom::WebUIGraphDumpRequest request) {
  binding_.Bind(std::move(request));
  binding_.set_connection_error_handler(base::BindOnce(
      &WebUIGraphDumpImpl::OnConnectionError, base::Unretained(this)));
}

namespace {

template <typename FunctionType>
void ForFrameAndOffspring(const FrameNode* parent_frame,
                          FunctionType on_frame) {
  on_frame(parent_frame);

  for (const FrameNode* child_frame : parent_frame->GetChildFrameNodes())
    ForFrameAndOffspring(child_frame, on_frame);
}

}  // namespace

void WebUIGraphDumpImpl::SubscribeToChanges(
    mojom::WebUIGraphChangeStreamPtr change_subscriber) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  change_subscriber_ = std::move(change_subscriber);

  // Send creation notifications for all existing nodes.
  for (const ProcessNode* process_node : graph_->GetAllProcessNodes())
    SendProcessNotification(process_node, true);

  for (const PageNode* page_node : graph_->GetAllPageNodes()) {
    SendPageNotification(page_node, true);
    StartPageFaviconRequest(page_node);

    // Dispatch preorder frame notifications.
    for (const FrameNode* main_frame_node : page_node->GetMainFrameNodes()) {
      ForFrameAndOffspring(main_frame_node,
                           [this](const FrameNode* frame_node) {
                             this->SendFrameNotification(frame_node, true);
                             this->StartFrameFaviconRequest(frame_node);
                           });
    }
  }

  // Subscribe to subsequent notifications.
  graph_->AddFrameNodeObserver(this);
  graph_->AddPageNodeObserver(this);
  graph_->AddProcessNodeObserver(this);
}

void WebUIGraphDumpImpl::OnPassedToGraph(Graph* graph) {
  DCHECK(!graph_);
  graph_ = graph;
}

void WebUIGraphDumpImpl::OnTakenFromGraph(Graph* graph) {
  DCHECK_EQ(graph_, graph);

  if (change_subscriber_) {
    graph_->RemoveFrameNodeObserver(this);
    graph_->RemovePageNodeObserver(this);
    graph_->RemoveProcessNodeObserver(this);
  }

  change_subscriber_.reset();

  // The favicon helper must be deleted on the UI thread.
  if (favicon_request_helper_) {
    content::BrowserThread::DeleteSoon(content::BrowserThread::UI, FROM_HERE,
                                       std::move(favicon_request_helper_));
  }

  graph_ = nullptr;
}

void WebUIGraphDumpImpl::OnFrameNodeAdded(const FrameNode* frame_node) {
  SendFrameNotification(frame_node, true);
  StartFrameFaviconRequest(frame_node);
}

void WebUIGraphDumpImpl::OnBeforeFrameNodeRemoved(const FrameNode* frame_node) {
  SendDeletionNotification(frame_node);
}

void WebUIGraphDumpImpl::OnURLChanged(const FrameNode* frame_node) {
  SendFrameNotification(frame_node, false);
  StartFrameFaviconRequest(frame_node);
}

void WebUIGraphDumpImpl::OnPageNodeAdded(const PageNode* page_node) {
  SendPageNotification(page_node, true);
  StartPageFaviconRequest(page_node);
}

void WebUIGraphDumpImpl::OnBeforePageNodeRemoved(const PageNode* page_node) {
  SendDeletionNotification(page_node);
}

void WebUIGraphDumpImpl::OnFaviconUpdated(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StartPageFaviconRequest(page_node);
}

void WebUIGraphDumpImpl::OnMainFrameNavigationCommitted(
    const PageNode* page_node) {
  SendPageNotification(page_node, false);
  StartPageFaviconRequest(page_node);
}

void WebUIGraphDumpImpl::OnProcessNodeAdded(const ProcessNode* process_node) {
  SendProcessNotification(process_node, true);
}

void WebUIGraphDumpImpl::OnProcessLifetimeChange(
    const ProcessNode* process_node) {
  SendProcessNotification(process_node, false);
}

void WebUIGraphDumpImpl::OnBeforeProcessNodeRemoved(
    const ProcessNode* process_node) {
  SendDeletionNotification(process_node);
}

WebUIGraphDumpImpl::FaviconRequestHelper*
WebUIGraphDumpImpl::EnsureFaviconRequestHelper() {
  if (!favicon_request_helper_) {
    favicon_request_helper_ = std::make_unique<FaviconRequestHelper>(
        weak_factory_.GetWeakPtr(), base::SequencedTaskRunnerHandle::Get());
  }

  return favicon_request_helper_.get();
}

void WebUIGraphDumpImpl::StartPageFaviconRequest(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!page_node->GetMainFrameUrl().is_valid())
    return;

  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&FaviconRequestHelper::RequestFavicon,
                                base::Unretained(EnsureFaviconRequestHelper()),
                                page_node->GetMainFrameUrl(),
                                page_node->GetContentsProxy(),
                                Node::GetSerializationId(page_node)));
}

void WebUIGraphDumpImpl::StartFrameFaviconRequest(const FrameNode* frame_node) {
  if (!frame_node->GetURL().is_valid())
    return;

  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&FaviconRequestHelper::RequestFavicon,
                                base::Unretained(EnsureFaviconRequestHelper()),
                                frame_node->GetURL(),
                                frame_node->GetPageNode()->GetContentsProxy(),
                                Node::GetSerializationId(frame_node)));
}

void WebUIGraphDumpImpl::SendFrameNotification(const FrameNode* frame,
                                               bool created) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(https://crbug.com/961785): Add more frame properties.
  mojom::WebUIFrameInfoPtr frame_info = mojom::WebUIFrameInfo::New();

  frame_info->id = Node::GetSerializationId(frame);

  auto* parent_frame = frame->GetParentFrameNode();
  frame_info->parent_frame_id = Node::GetSerializationId(parent_frame);

  auto* process = frame->GetProcessNode();
  frame_info->process_id = Node::GetSerializationId(process);

  auto* page = frame->GetPageNode();
  frame_info->page_id = Node::GetSerializationId(page);

  frame_info->url = frame->GetURL();

  if (created)
    change_subscriber_->FrameCreated(std::move(frame_info));
  else
    change_subscriber_->FrameChanged(std::move(frame_info));
}

void WebUIGraphDumpImpl::SendPageNotification(const PageNode* page_node,
                                              bool created) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(https://crbug.com/961785): Add more page_node properties.
  mojom::WebUIPageInfoPtr page_info = mojom::WebUIPageInfo::New();

  page_info->id = Node::GetSerializationId(page_node);
  page_info->main_frame_url = page_node->GetMainFrameUrl();
  if (created)
    change_subscriber_->PageCreated(std::move(page_info));
  else
    change_subscriber_->PageChanged(std::move(page_info));
}

void WebUIGraphDumpImpl::SendProcessNotification(const ProcessNode* process,
                                                 bool created) {
  // TODO(https://crbug.com/961785): Add more process properties.
  mojom::WebUIProcessInfoPtr process_info = mojom::WebUIProcessInfo::New();

  process_info->id = Node::GetSerializationId(process);
  process_info->pid = process->GetProcessId();
  process_info->cumulative_cpu_usage = process->GetCumulativeCpuUsage();
  process_info->private_footprint_kb = process->GetPrivateFootprintKb();

  if (created)
    change_subscriber_->ProcessCreated(std::move(process_info));
  else
    change_subscriber_->ProcessChanged(std::move(process_info));
}

void WebUIGraphDumpImpl::SendDeletionNotification(const Node* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  change_subscriber_->NodeDeleted(Node::GetSerializationId(node));
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

// static
void WebUIGraphDumpImpl::OnConnectionError(WebUIGraphDumpImpl* impl) {
  std::unique_ptr<GraphOwned> owned_impl = impl->graph_->TakeFromGraph(impl);
}

}  // namespace performance_manager
