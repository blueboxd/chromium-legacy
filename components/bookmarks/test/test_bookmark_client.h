// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_TEST_TEST_BOOKMARK_CLIENT_H_
#define COMPONENTS_BOOKMARKS_TEST_TEST_BOOKMARK_CLIENT_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "components/bookmarks/browser/bookmark_client.h"

namespace bookmarks {

class BookmarkModel;

class TestBookmarkClient : public BookmarkClient {
 public:
  TestBookmarkClient();
  ~TestBookmarkClient() override;

  // Returns a new BookmarkModel using a TestBookmarkClient.
  static std::unique_ptr<BookmarkModel> CreateModel();

  // Returns a new BookmarkModel using |client|.
  static std::unique_ptr<BookmarkModel> CreateModelWithClient(
      std::unique_ptr<BookmarkClient> client);

  // Sets the extra node to be returned by the next call to CreateModel() or
  // GetLoadExtraNodeCallback().
  void SetExtraNodeToLoad(std::unique_ptr<BookmarkPermanentNode> extra_node);

  // Returns the current extra_node, set via SetExtraNodeToLoad().
  BookmarkPermanentNode* extra_node() { return unowned_extra_node_; }

  // Returns true if |node| is the |extra_node_|.
  bool IsExtraNodeRoot(const BookmarkNode* node);

  // Returns true if |node| belongs to the tree of the |extra_node_|.
  bool IsAnExtraNode(const BookmarkNode* node);

 private:
  // BookmarkClient:
  bool IsPermanentNodeVisible(const BookmarkPermanentNode* node) override;
  void RecordAction(const base::UserMetricsAction& action) override;
  LoadExtraCallback GetLoadExtraNodeCallback() override;
  bool CanSetPermanentNodeTitle(const BookmarkNode* permanent_node) override;
  bool CanSyncNode(const BookmarkNode* node) override;
  bool CanBeEditedByUser(const BookmarkNode* node) override;
  std::string EncodeBookmarkSyncMetadata() override;
  void DecodeBookmarkSyncMetadata(
      const std::string& metadata_str,
      const base::RepeatingClosure& schedule_save_closure) override;

  // Helpers for GetLoadExtraNodeCallback().
  static std::unique_ptr<BookmarkPermanentNode> LoadExtraNode(
      std::unique_ptr<BookmarkPermanentNode> extra_node,
      int64_t* next_id);

  // extra_node_ exists only until GetLoadExtraNodeCallback gets called, but
  // unowned_extra_node_ stays around after that.
  std::unique_ptr<BookmarkPermanentNode> extra_node_;
  BookmarkPermanentNode* unowned_extra_node_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestBookmarkClient);
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_TEST_TEST_BOOKMARK_CLIENT_H_
