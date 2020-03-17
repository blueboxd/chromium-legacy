// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/stream_model.h"

#include "base/optional.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/content_id.pb.h"
#include "components/feed/core/v2/test/stream_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

std::vector<std::string> GetContentFrames(const StreamModel& model) {
  std::vector<std::string> frames;
  for (ContentRevision rev : model.GetContentList()) {
    const feedstore::Content* content = model.FindContent(rev);
    if (content) {
      frames.push_back(content->frame());
    } else {
      frames.push_back("<null>");
    }
  }
  return frames;
}

class TestObserver : public StreamModel::Observer {
 public:
  explicit TestObserver(StreamModel* model) { model->SetObserver(this); }
  void OnUiUpdate(const StreamModel::UiUpdate& update) override {
    update_ = update;
  }

  const base::Optional<StreamModel::UiUpdate>& GetUiUpdate() const {
    return update_;
  }
  bool ContentListChanged() const {
    return update_ && update_->content_list_changed;
  }
  void Clear() { update_ = base::nullopt; }

 private:
  base::Optional<StreamModel::UiUpdate> update_;
};

TEST(StreamModelTest, ConstructEmptyModel) {
  StreamModel model;
  TestObserver observer(&model);

  EXPECT_EQ(0UL, model.GetContentList().size());
}

// Typical stream (Stream -> Cluster -> Content).
TEST(StreamModelTest, AddStreamClusterContent) {
  StreamModel model;
  TestObserver observer(&model);

  model.ExecuteOperations(MakeTypicalStreamOperations());

  EXPECT_TRUE(observer.ContentListChanged());
  EXPECT_EQ(std::vector<std::string>({"f:0", "f:1"}), GetContentFrames(model));
}

TEST(StreamModelTest, AddContentWithoutRoot) {
  StreamModel model;
  TestObserver observer(&model);

  std::vector<feedstore::DataOperation> operations{
      MakeOperation(MakeCluster(0, MakeRootId())),
      MakeOperation(MakeContentNode(0, MakeClusterId(0))),
      MakeOperation(MakeContent(0)),
  };
  model.ExecuteOperations(operations);

  // Without a root, no content is visible.
  EXPECT_EQ(std::vector<std::string>({}), GetContentFrames(model));
}

// Verify Stream -> Content works.
TEST(StreamModelTest, AddStreamContent) {
  StreamModel model;
  TestObserver observer(&model);

  std::vector<feedstore::DataOperation> operations{
      MakeOperation(MakeStream()),
      MakeOperation(MakeContentNode(0, MakeRootId())),
      MakeOperation(MakeContent(0)),
  };
  model.ExecuteOperations(operations);

  EXPECT_EQ(std::vector<std::string>({"f:0"}), GetContentFrames(model));
}

TEST(StreamModelTest, AddRootAsChild) {
  // When the root is added as a child, it's no longer considered a root.
  StreamModel model;
  TestObserver observer(&model);
  feedstore::StreamStructure stream_with_parent = MakeStream();
  *stream_with_parent.mutable_parent_id() = MakeContentContentId(0);
  std::vector<feedstore::DataOperation> operations{
      MakeOperation(MakeStream()),
      MakeOperation(MakeContentNode(0, MakeRootId())),
      MakeOperation(MakeContent(0)),
      MakeOperation(stream_with_parent),
  };

  model.ExecuteOperations(operations);

  EXPECT_EQ(std::vector<std::string>({}), GetContentFrames(model));
}

// Changing the STREAM root to CLUSTER means it is no longer eligible to be
// the root.
TEST(StreamModelTest, ChangeStreamToCluster) {
  StreamModel model;
  TestObserver observer(&model);
  feedstore::StreamStructure stream_as_cluster = MakeStream();
  stream_as_cluster.set_type(feedstore::StreamStructure::CLUSTER);

  std::vector<feedstore::DataOperation> operations{
      MakeOperation(MakeStream()),
      MakeOperation(MakeContentNode(0, MakeRootId())),
      MakeOperation(MakeContent(0)),
      MakeOperation(stream_as_cluster),
  };

  model.ExecuteOperations(operations);

  EXPECT_EQ(std::vector<std::string>({}), GetContentFrames(model));
}

TEST(StreamModelTest, RemoveCluster) {
  StreamModel model;
  TestObserver observer(&model);

  std::vector<feedstore::DataOperation> operations =
      MakeTypicalStreamOperations();
  operations.push_back(MakeOperation(MakeRemove(MakeClusterId(0))));

  model.ExecuteOperations(operations);

  EXPECT_EQ(std::vector<std::string>({"f:1"}), GetContentFrames(model));
}

TEST(StreamModelTest, RemoveContent) {
  StreamModel model;
  TestObserver observer(&model);

  std::vector<feedstore::DataOperation> operations =
      MakeTypicalStreamOperations();
  operations.push_back(MakeOperation(MakeRemove(MakeContentContentId(0))));

  model.ExecuteOperations(operations);

  EXPECT_EQ(std::vector<std::string>({"f:1"}), GetContentFrames(model));
}

TEST(StreamModelTest, RemoveRoot) {
  StreamModel model;
  TestObserver observer(&model);

  std::vector<feedstore::DataOperation> operations =
      MakeTypicalStreamOperations();
  operations.push_back(MakeOperation(MakeRemove(MakeRootId())));

  model.ExecuteOperations(operations);

  EXPECT_EQ(std::vector<std::string>(), GetContentFrames(model));
}

TEST(StreamModelTest, RemoveAndAddRoot) {
  StreamModel model;
  TestObserver observer(&model);

  std::vector<feedstore::DataOperation> operations =
      MakeTypicalStreamOperations();
  operations.push_back(MakeOperation(MakeRemove(MakeRootId())));
  operations.push_back(MakeOperation(MakeStream()));

  model.ExecuteOperations(operations);

  EXPECT_EQ(std::vector<std::string>({"f:0", "f:1"}), GetContentFrames(model));
}

TEST(StreamModelTest, SwitchStreams) {
  StreamModel model;
  TestObserver observer(&model);

  std::vector<feedstore::DataOperation> operations =
      MakeTypicalStreamOperations();
  operations.push_back(MakeOperation(MakeStream(2)));
  operations.push_back(MakeOperation(MakeContentNode(9, MakeRootId(2))));
  operations.push_back(MakeOperation(MakeContent(9)));

  model.ExecuteOperations(operations);

  // The last stream added becomes the root, so only children of 'root2' are
  // included.
  EXPECT_EQ(std::vector<std::string>({"f:9"}), GetContentFrames(model));

  // Adding the original stream back will re-activate it.
  model.ExecuteOperations({MakeOperation(MakeStream())});

  EXPECT_EQ(std::vector<std::string>({"f:0", "f:1"}), GetContentFrames(model));

  // Removing 'root' will now make 'root2' active again.
  model.ExecuteOperations({MakeOperation(MakeRemove(MakeRootId()))});
  EXPECT_EQ(std::vector<std::string>({"f:9"}), GetContentFrames(model));
}

TEST(StreamModelTest, RemoveAndUpdateCluster) {
  // Remove a cluster and add it back. Adding it back keeps its original
  // placement.
  StreamModel model;
  TestObserver observer(&model);

  std::vector<feedstore::DataOperation> operations =
      MakeTypicalStreamOperations();
  operations.push_back(MakeOperation(MakeRemove(MakeClusterId(0))));
  operations.push_back(MakeOperation(MakeCluster(0, MakeRootId())));

  model.ExecuteOperations(operations);

  EXPECT_EQ(std::vector<std::string>({"f:0", "f:1"}), GetContentFrames(model));
}

TEST(StreamModelTest, RemoveAndAppendToNewParent) {
  // Attempt to re-parent a node. This is not allowed, the old parent remains.
  StreamModel model;
  TestObserver observer(&model);

  std::vector<feedstore::DataOperation> operations =
      MakeTypicalStreamOperations();
  operations.push_back(MakeOperation(MakeRemove(MakeClusterId(0))));
  operations.push_back(MakeOperation(MakeCluster(0, MakeClusterId(1))));

  model.ExecuteOperations(operations);

  EXPECT_EQ(std::vector<std::string>({"f:0", "f:1"}), GetContentFrames(model));
}

TEST(StreamModelTest, EphemeralNewCluster) {
  StreamModel model;
  TestObserver observer(&model);

  model.ExecuteOperations(MakeTypicalStreamOperations());
  observer.Clear();

  model.CreateEphemeralChange({
      MakeOperation(MakeCluster(2, MakeRootId())),
      MakeOperation(MakeContentNode(2, MakeClusterId(2))),
      MakeOperation(MakeContent(2)),
  });

  EXPECT_TRUE(observer.ContentListChanged());
  EXPECT_EQ(std::vector<std::string>({"f:0", "f:1", "f:2"}),
            GetContentFrames(model));
}

TEST(StreamModelTest, CommitEphemeralChange) {
  StreamModel model;
  TestObserver observer(&model);

  model.ExecuteOperations(MakeTypicalStreamOperations());
  EphemeralChangeId change_id = model.CreateEphemeralChange({
      MakeOperation(MakeCluster(2, MakeRootId())),
      MakeOperation(MakeContentNode(2, MakeClusterId(2))),
      MakeOperation(MakeContent(2)),
  });

  EXPECT_TRUE(model.CommitEphemeralChange(change_id));

  // Can't reject after commit.
  EXPECT_FALSE(model.RejectEphemeralChange(change_id));

  EXPECT_EQ(std::vector<std::string>({"f:0", "f:1", "f:2"}),
            GetContentFrames(model));
}

TEST(StreamModelTest, RejectEphemeralChange) {
  StreamModel model;
  TestObserver observer(&model);

  model.ExecuteOperations(MakeTypicalStreamOperations());
  EphemeralChangeId change_id = model.CreateEphemeralChange({
      MakeOperation(MakeCluster(2, MakeRootId())),
      MakeOperation(MakeContentNode(2, MakeClusterId(2))),
      MakeOperation(MakeContent(2)),
  });
  observer.Clear();

  EXPECT_TRUE(model.RejectEphemeralChange(change_id));
  EXPECT_TRUE(observer.ContentListChanged());
  // Can't commit after reject.
  EXPECT_FALSE(model.CommitEphemeralChange(change_id));

  EXPECT_EQ(std::vector<std::string>({"f:0", "f:1"}), GetContentFrames(model));
}

TEST(StreamModelTest, RejectFirstEphemeralChange) {
  StreamModel model;
  TestObserver observer(&model);

  model.ExecuteOperations(MakeTypicalStreamOperations());
  EphemeralChangeId change_id1 = model.CreateEphemeralChange({
      MakeOperation(MakeCluster(2, MakeRootId())),
      MakeOperation(MakeContentNode(2, MakeClusterId(2))),
      MakeOperation(MakeContent(2)),
  });

  model.CreateEphemeralChange({
      MakeOperation(MakeCluster(3, MakeRootId())),
      MakeOperation(MakeContentNode(3, MakeClusterId(3))),
      MakeOperation(MakeContent(3)),
  });
  observer.Clear();

  EXPECT_TRUE(model.RejectEphemeralChange(change_id1));
  EXPECT_TRUE(observer.ContentListChanged());
  // Can't commit after reject.
  EXPECT_FALSE(model.CommitEphemeralChange(change_id1));

  EXPECT_EQ(std::vector<std::string>({"f:0", "f:1", "f:3"}),
            GetContentFrames(model));
}

}  // namespace
}  // namespace feed
