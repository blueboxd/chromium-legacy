// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/test/test_bookmark_client.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_storage.h"

namespace bookmarks {

TestBookmarkClient::TestBookmarkClient() {}

TestBookmarkClient::~TestBookmarkClient() {}

// static
std::unique_ptr<BookmarkModel> TestBookmarkClient::CreateModel() {
  return CreateModelWithClient(base::WrapUnique(new TestBookmarkClient));
}

// static
std::unique_ptr<BookmarkModel> TestBookmarkClient::CreateModelWithClient(
    std::unique_ptr<BookmarkClient> client) {
  BookmarkClient* client_ptr = client.get();
  std::unique_ptr<BookmarkModel> bookmark_model(
      new BookmarkModel(std::move(client)));
  std::unique_ptr<BookmarkLoadDetails> details =
      std::make_unique<BookmarkLoadDetails>(client_ptr);
  details->LoadExtraNode();
  details->CreateUrlIndex();
  bookmark_model->DoneLoading(std::move(details));
  return bookmark_model;
}

void TestBookmarkClient::SetExtraNodeToLoad(
    std::unique_ptr<BookmarkPermanentNode> extra_node) {
  extra_node_ = std::move(extra_node);
  // Keep a copy of the node in |unowned_extra_node_| for the accessor
  // functions.
  unowned_extra_node_ = extra_node_.get();
}

bool TestBookmarkClient::IsExtraNodeRoot(const BookmarkNode* node) {
  return unowned_extra_node_ == node;
}

bool TestBookmarkClient::IsAnExtraNode(const BookmarkNode* node) {
  return node && node->HasAncestor(unowned_extra_node_);
}

bool TestBookmarkClient::IsPermanentNodeVisible(
    const BookmarkPermanentNode* node) {
  DCHECK(node->type() == BookmarkNode::BOOKMARK_BAR ||
         node->type() == BookmarkNode::OTHER_NODE ||
         node->type() == BookmarkNode::MOBILE ||
         IsExtraNodeRoot(node));
  return node->type() != BookmarkNode::MOBILE && !IsExtraNodeRoot(node);
}

void TestBookmarkClient::RecordAction(const base::UserMetricsAction& action) {
}

LoadExtraCallback TestBookmarkClient::GetLoadExtraNodeCallback() {
  return base::BindOnce(&TestBookmarkClient::LoadExtraNode,
                        std::move(extra_node_));
}

bool TestBookmarkClient::CanSetPermanentNodeTitle(
    const BookmarkNode* permanent_node) {
  return IsExtraNodeRoot(permanent_node);
}

bool TestBookmarkClient::CanSyncNode(const BookmarkNode* node) {
  return !IsAnExtraNode(node);
}

bool TestBookmarkClient::CanBeEditedByUser(const BookmarkNode* node) {
  return !IsAnExtraNode(node);
}

std::string TestBookmarkClient::EncodeBookmarkSyncMetadata() {
  return std::string();
}

void TestBookmarkClient::DecodeBookmarkSyncMetadata(
    const std::string& metadata_str,
    const base::RepeatingClosure& schedule_save_closure) {}

// static
std::unique_ptr<BookmarkPermanentNode> TestBookmarkClient::LoadExtraNode(
    std::unique_ptr<BookmarkPermanentNode> extra_node,
    int64_t* next_id) {
  return extra_node;
}

}  // namespace bookmarks
