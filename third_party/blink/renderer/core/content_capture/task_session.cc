// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/content_capture/task_session.h"

#include <utility>

#include "third_party/blink/renderer/core/content_capture/content_holder.h"
#include "third_party/blink/renderer/core/content_capture/sent_nodes.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"

namespace blink {

TaskSession::DocumentSession::DocumentSession(const Document& document,
                                              SentNodes& sent_nodes,
                                              SentNodeCountCallback& callback)
    : document_(&document), sent_nodes_(&sent_nodes), callback_(callback) {}

TaskSession::DocumentSession::~DocumentSession() {
  if (callback_.has_value())
    callback_.value().Run(total_sent_nodes_);
}

void TaskSession::DocumentSession::AddNodeHolder(cc::NodeHolder node_holder) {
  captured_content_.push_back(node_holder);
}

void TaskSession::DocumentSession::AddDetachedNode(int64_t id) {
  detached_nodes_.emplace_back(id);
}

void TaskSession::DocumentSession::AddChangedNodeHolder(
    cc::NodeHolder node_holder) {
  changed_content_.push_back(node_holder);
}

WebVector<int64_t> TaskSession::DocumentSession::MoveDetachedNodes() {
  return std::move(detached_nodes_);
}

scoped_refptr<blink::ContentHolder>
TaskSession::DocumentSession::GetNextUnsentContentHolder() {
  scoped_refptr<ContentHolder> content_holder;
  while (!captured_content_.IsEmpty() && !content_holder) {
    auto node_holder = captured_content_.back();
    if (node_holder.type == cc::NodeHolder::Type::kID) {
      Node* node = DOMNodeIds::NodeForId(node_holder.id);
      if (node && node->GetLayoutObject() && !sent_nodes_->HasSent(*node)) {
        sent_nodes_->OnSent(*node);
        content_holder = base::MakeRefCounted<ContentHolder>(*node);
      }
    } else if (node_holder.type == cc::NodeHolder::Type::kTextHolder &&
               node_holder.text_holder) {
      content_holder = scoped_refptr<ContentHolder>(
          static_cast<ContentHolder*>(node_holder.text_holder.get()));
      if (content_holder && content_holder->IsValid() &&
          !content_holder->HasSent()) {
        content_holder->SetHasSent();
      } else {
        content_holder.reset();
      }
    }
    captured_content_.pop_back();
  }
  if (content_holder)
    total_sent_nodes_++;
  return content_holder;
}

scoped_refptr<blink::ContentHolder>
TaskSession::DocumentSession::GetNextChangedContentHolder() {
  scoped_refptr<ContentHolder> content_holder;
  while (!changed_content_.IsEmpty() && !content_holder) {
    auto node_holder = changed_content_.back();
    if (node_holder.type == cc::NodeHolder::Type::kID) {
      Node* node = DOMNodeIds::NodeForId(node_holder.id);
      if (node && node->GetLayoutObject())
        content_holder = base::MakeRefCounted<ContentHolder>(*node);
    } else if (node_holder.type == cc::NodeHolder::Type::kTextHolder &&
               node_holder.text_holder) {
      content_holder = scoped_refptr<ContentHolder>(
          static_cast<ContentHolder*>(node_holder.text_holder.get()));
      if (content_holder && !content_holder->IsValid())
        content_holder.reset();
    }
    changed_content_.pop_back();
  }
  if (content_holder)
    total_sent_nodes_++;
  return content_holder;
}

void TaskSession::DocumentSession::Trace(blink::Visitor* visitor) {
  visitor->Trace(sent_nodes_);
  visitor->Trace(document_);
}

void TaskSession::DocumentSession::Reset() {
  changed_content_.clear();
  captured_content_.clear();
  detached_nodes_.Clear();
}

TaskSession::TaskSession(SentNodes& sent_nodes) : sent_nodes_(sent_nodes) {}

TaskSession::DocumentSession* TaskSession::GetNextUnsentDocumentSession() {
  for (auto& doc : to_document_session_.Values()) {
    if (!doc->HasUnsentData())
      continue;
    return doc;
  }
  has_unsent_data_ = false;
  return nullptr;
}

void TaskSession::SetCapturedContent(
    const Vector<cc::NodeHolder>& captured_content) {
  DCHECK(!HasUnsentData());
  DCHECK(!captured_content.IsEmpty());
  GroupCapturedContentByDocument(captured_content);
  has_unsent_data_ = true;
}

void TaskSession::GroupCapturedContentByDocument(
    const Vector<cc::NodeHolder>& captured_content) {
  for (const cc::NodeHolder& node_holder : captured_content) {
    if (const Node* node = GetNode(node_holder)) {
      node = changed_nodes_.Take(node);
      if (node) {
        // The changed node might not be sent.
        if (GetNodeIf(true, node_holder)) {
          EnsureDocumentSession(node->GetDocument())
              .AddChangedNodeHolder(node_holder);
        } else {
          EnsureDocumentSession(node->GetDocument()).AddNodeHolder(node_holder);
        }
        continue;
      }
    }
    if (const Node* node = GetNodeIf(false /* sent */, node_holder)) {
      EnsureDocumentSession(node->GetDocument()).AddNodeHolder(node_holder);
    }
  }
}

void TaskSession::OnNodeDetached(const cc::NodeHolder& node_holder) {
  if (const Node* node = GetNodeIf(true /* sent */, node_holder)) {
    EnsureDocumentSession(node->GetDocument())
        .AddDetachedNode(reinterpret_cast<int64_t>(node));
    has_unsent_data_ = true;
  }
}

void TaskSession::OnNodeChanged(const cc::NodeHolder& node_holder) {
  if (const Node* node = GetNode(node_holder)) {
    changed_nodes_.insert(WeakMember<const Node>(node));
  }
}

const Node* TaskSession::GetNodeIf(bool sent,
                                   const cc::NodeHolder& node_holder) const {
  Node* node = nullptr;
  if (node_holder.type == cc::NodeHolder::Type::kID) {
    node = DOMNodeIds::NodeForId(node_holder.id);
    if (node && (sent_nodes_->HasSent(*node) == sent))
      return node;
  } else if (node_holder.type == cc::NodeHolder::Type::kTextHolder) {
    ContentHolder* content_holder =
        static_cast<ContentHolder*>(node_holder.text_holder.get());
    if (content_holder && content_holder->IsValid() &&
        (content_holder->HasSent() == sent)) {
      return content_holder->GetNode();
    }
  }
  return nullptr;
}

const Node* TaskSession::GetNode(const cc::NodeHolder& node_holder) const {
  if (node_holder.type == cc::NodeHolder::Type::kID)
    return DOMNodeIds::NodeForId(node_holder.id);

  if (node_holder.type == cc::NodeHolder::Type::kTextHolder) {
    ContentHolder* content_holder =
        static_cast<ContentHolder*>(node_holder.text_holder.get());
    if (content_holder)
      return content_holder->GetNode();
  }
  return nullptr;
}

TaskSession::DocumentSession& TaskSession::EnsureDocumentSession(
    const Document& doc) {
  DocumentSession* doc_session = GetDocumentSession(doc);
  if (!doc_session) {
    doc_session =
        MakeGarbageCollected<DocumentSession>(doc, *sent_nodes_, callback_);
    to_document_session_.insert(&doc, doc_session);
  }
  return *doc_session;
}

TaskSession::DocumentSession* TaskSession::GetDocumentSession(
    const Document& document) const {
  auto it = to_document_session_.find(&document);
  if (it == to_document_session_.end())
    return nullptr;
  return it->value;
}

void TaskSession::Trace(blink::Visitor* visitor) {
  visitor->Trace(sent_nodes_);
  visitor->Trace(changed_nodes_);
  visitor->Trace(to_document_session_);
}

void TaskSession::ClearDocumentSessionsForTesting() {
  to_document_session_.clear();
}

}  // namespace blink
