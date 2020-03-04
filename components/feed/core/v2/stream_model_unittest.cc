// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/stream_model.h"

#include "base/optional.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/wire/content_id.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {
using feedwire::ContentId;
using EphemeralChangeId = StreamModel::EphemeralChangeId;
using ContentRevision = StreamModel::ContentRevision;

ContentId MakeContentId(const std::string& domain, const std::string& table) {
  ContentId id;
  id.set_content_domain(domain);
  id.set_table(table);
  return id;
}

ContentId MakeClusterId(const std::string& name) {
  return MakeContentId("cluster", name);
}

ContentId MakeContentContentId(const std::string& name) {
  return MakeContentId("content", name);
}
ContentId MakeRootId(const std::string& name = "root") {
  return MakeContentId("stream", name);
}

feedstore::StreamStructure MakeStream(const std::string& name = "root") {
  feedstore::StreamStructure result;
  result.set_type(feedstore::StreamStructure::STREAM);
  result.set_operation(feedstore::StreamStructure::UPDATE_OR_APPEND);
  *result.mutable_content_id() = MakeRootId(name);
  return result;
}

feedstore::StreamStructure MakeCluster(const std::string& name,
                                       ContentId parent) {
  feedstore::StreamStructure result;
  result.set_type(feedstore::StreamStructure::CLUSTER);
  result.set_operation(feedstore::StreamStructure::UPDATE_OR_APPEND);
  *result.mutable_content_id() = MakeClusterId(name);
  *result.mutable_parent_id() = parent;
  return result;
}

feedstore::StreamStructure MakeContentNode(const std::string& name,
                                           ContentId parent) {
  feedstore::StreamStructure result;
  result.set_type(feedstore::StreamStructure::CONTENT);
  result.set_operation(feedstore::StreamStructure::UPDATE_OR_APPEND);
  *result.mutable_content_id() = MakeContentContentId(name);
  *result.mutable_parent_id() = parent;
  return result;
}

feedstore::StreamStructure MakeRemove(ContentId id) {
  feedstore::StreamStructure result;
  result.set_operation(feedstore::StreamStructure::REMOVE);
  *result.mutable_content_id() = id;
  return result;
}

feedstore::Content MakeContent(const std::string& name) {
  feedstore::Content result;
  *result.mutable_content_id() = MakeContentContentId(name);
  result.set_frame("f:" + name);
  return result;
}

feedstore::DataOperation MakeOperation(feedstore::StreamStructure structure) {
  feedstore::DataOperation operation;
  *operation.mutable_structure() = std::move(structure);
  return operation;
}

feedstore::DataOperation MakeOperation(feedstore::Content content) {
  feedstore::DataOperation operation;
  *operation.mutable_content() = std::move(content);
  return operation;
}

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

std::vector<feedstore::DataOperation> MakeTypicalStreamOperations() {
  return {
      MakeOperation(MakeStream()),
      MakeOperation(MakeCluster("A", MakeRootId())),
      MakeOperation(MakeContentNode("A", MakeClusterId("A"))),
      MakeOperation(MakeContent("A")),
      MakeOperation(MakeCluster("B", MakeRootId())),
      MakeOperation(MakeContentNode("B", MakeClusterId("B"))),
      MakeOperation(MakeContent("B")),
  };
}

class TestObserver : public StreamModel::Observer {
 public:
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
  TestObserver observer;
  StreamModel model(&observer);

  EXPECT_EQ(0UL, model.GetContentList().size());
}

// Typical stream (Stream -> Cluster -> Content).
TEST(StreamModelTest, AddStreamClusterContent) {
  TestObserver observer;
  StreamModel model(&observer);

  model.ExecuteOperations(MakeTypicalStreamOperations());

  EXPECT_TRUE(observer.ContentListChanged());
  EXPECT_EQ(std::vector<std::string>({"f:A", "f:B"}), GetContentFrames(model));
}

TEST(StreamModelTest, AddContentWithoutRoot) {
  TestObserver observer;
  StreamModel model(&observer);

  std::vector<feedstore::DataOperation> operations{
      MakeOperation(MakeCluster("A", MakeRootId())),
      MakeOperation(MakeContentNode("A", MakeClusterId("A"))),
      MakeOperation(MakeContent("A")),
  };
  model.ExecuteOperations(operations);

  // Without a root, no content is visible.
  EXPECT_EQ(std::vector<std::string>({}), GetContentFrames(model));
}

// Verify Stream -> Content works.
TEST(StreamModelTest, AddStreamContent) {
  TestObserver observer;
  StreamModel model(&observer);

  std::vector<feedstore::DataOperation> operations{
      MakeOperation(MakeStream()),
      MakeOperation(MakeContentNode("A", MakeRootId())),
      MakeOperation(MakeContent("A")),
  };
  model.ExecuteOperations(operations);

  EXPECT_EQ(std::vector<std::string>({"f:A"}), GetContentFrames(model));
}

TEST(StreamModelTest, AddRootAsChild) {
  // When the root is added as a child, it's no longer considered a root.
  TestObserver observer;
  StreamModel model(&observer);
  feedstore::StreamStructure stream_with_parent = MakeStream();
  *stream_with_parent.mutable_parent_id() = MakeContentContentId("A");
  std::vector<feedstore::DataOperation> operations{
      MakeOperation(MakeStream()),
      MakeOperation(MakeContentNode("A", MakeRootId())),
      MakeOperation(MakeContent("A")),
      MakeOperation(stream_with_parent),
  };

  model.ExecuteOperations(operations);

  EXPECT_EQ(std::vector<std::string>({}), GetContentFrames(model));
}

// Changing the STREAM root to CLUSTER means it is no longer eligible to be
// the root.
TEST(StreamModelTest, ChangeStreamToCluster) {
  TestObserver observer;
  StreamModel model(&observer);
  feedstore::StreamStructure stream_as_cluster = MakeStream();
  stream_as_cluster.set_type(feedstore::StreamStructure::CLUSTER);

  std::vector<feedstore::DataOperation> operations{
      MakeOperation(MakeStream()),
      MakeOperation(MakeContentNode("A", MakeRootId())),
      MakeOperation(MakeContent("A")),
      MakeOperation(stream_as_cluster),
  };

  model.ExecuteOperations(operations);

  EXPECT_EQ(std::vector<std::string>({}), GetContentFrames(model));
}

TEST(StreamModelTest, RemoveCluster) {
  TestObserver observer;
  StreamModel model(&observer);

  std::vector<feedstore::DataOperation> operations =
      MakeTypicalStreamOperations();
  operations.push_back(MakeOperation(MakeRemove(MakeClusterId("A"))));

  model.ExecuteOperations(operations);

  EXPECT_EQ(std::vector<std::string>({"f:B"}), GetContentFrames(model));
}

TEST(StreamModelTest, RemoveContent) {
  TestObserver observer;
  StreamModel model(&observer);

  std::vector<feedstore::DataOperation> operations =
      MakeTypicalStreamOperations();
  operations.push_back(MakeOperation(MakeRemove(MakeContentContentId("A"))));

  model.ExecuteOperations(operations);

  EXPECT_EQ(std::vector<std::string>({"f:B"}), GetContentFrames(model));
}

TEST(StreamModelTest, RemoveRoot) {
  TestObserver observer;
  StreamModel model(&observer);

  std::vector<feedstore::DataOperation> operations =
      MakeTypicalStreamOperations();
  operations.push_back(MakeOperation(MakeRemove(MakeRootId())));

  model.ExecuteOperations(operations);

  EXPECT_EQ(std::vector<std::string>(), GetContentFrames(model));
}

TEST(StreamModelTest, RemoveAndAddRoot) {
  TestObserver observer;
  StreamModel model(&observer);

  std::vector<feedstore::DataOperation> operations =
      MakeTypicalStreamOperations();
  operations.push_back(MakeOperation(MakeRemove(MakeRootId())));
  operations.push_back(MakeOperation(MakeStream()));

  model.ExecuteOperations(operations);

  EXPECT_EQ(std::vector<std::string>({"f:A", "f:B"}), GetContentFrames(model));
}

TEST(StreamModelTest, SwitchStreams) {
  TestObserver observer;
  StreamModel model(&observer);

  std::vector<feedstore::DataOperation> operations =
      MakeTypicalStreamOperations();
  operations.push_back(MakeOperation(MakeStream("root2")));
  operations.push_back(
      MakeOperation(MakeContentNode("F", MakeRootId("root2"))));
  operations.push_back(MakeOperation(MakeContent("F")));

  model.ExecuteOperations(operations);

  // The last stream added becomes the root, so only children of 'root2' are
  // included.
  EXPECT_EQ(std::vector<std::string>({"f:F"}), GetContentFrames(model));

  // Adding the original stream back will re-activate it.
  model.ExecuteOperations({MakeOperation(MakeStream())});

  EXPECT_EQ(std::vector<std::string>({"f:A", "f:B"}), GetContentFrames(model));

  // Removing 'root' will now make 'root2' active again.
  model.ExecuteOperations({MakeOperation(MakeRemove(MakeRootId()))});
  EXPECT_EQ(std::vector<std::string>({"f:F"}), GetContentFrames(model));
  LOG(ERROR) << "VS:" << sizeof(std::vector<int>);
}

TEST(StreamModelTest, RemoveAndUpdateCluster) {
  // Remove a cluster and add it back. Adding it back keeps its original
  // placement.
  TestObserver observer;
  StreamModel model(&observer);

  std::vector<feedstore::DataOperation> operations =
      MakeTypicalStreamOperations();
  operations.push_back(MakeOperation(MakeRemove(MakeClusterId("A"))));
  operations.push_back(MakeOperation(MakeCluster("A", MakeRootId())));

  model.ExecuteOperations(operations);

  EXPECT_EQ(std::vector<std::string>({"f:A", "f:B"}), GetContentFrames(model));
}

TEST(StreamModelTest, RemoveAndAppendToNewParent) {
  // Attempt to re-parent a node. This is not allowed, the old parent remains.
  TestObserver observer;
  StreamModel model(&observer);

  std::vector<feedstore::DataOperation> operations =
      MakeTypicalStreamOperations();
  operations.push_back(MakeOperation(MakeRemove(MakeClusterId("A"))));
  operations.push_back(MakeOperation(MakeCluster("A", MakeClusterId("B"))));

  model.ExecuteOperations(operations);

  EXPECT_EQ(std::vector<std::string>({"f:A", "f:B"}), GetContentFrames(model));
}

TEST(StreamModelTest, EphemeralNewCluster) {
  TestObserver observer;
  StreamModel model(&observer);

  model.ExecuteOperations(MakeTypicalStreamOperations());
  observer.Clear();

  model.CreateEphemeralChange({
      MakeOperation(MakeCluster("C", MakeRootId())),
      MakeOperation(MakeContentNode("C", MakeClusterId("C"))),
      MakeOperation(MakeContent("C")),
  });

  EXPECT_TRUE(observer.ContentListChanged());
  EXPECT_EQ(std::vector<std::string>({"f:A", "f:B", "f:C"}),
            GetContentFrames(model));
}

TEST(StreamModelTest, CommitEphemeralChange) {
  TestObserver observer;
  StreamModel model(&observer);

  model.ExecuteOperations(MakeTypicalStreamOperations());
  EphemeralChangeId change_id = model.CreateEphemeralChange({
      MakeOperation(MakeCluster("C", MakeRootId())),
      MakeOperation(MakeContentNode("C", MakeClusterId("C"))),
      MakeOperation(MakeContent("C")),
  });

  EXPECT_TRUE(model.CommitEphemeralChange(change_id));

  // Can't reject after commit.
  EXPECT_FALSE(model.RejectEphemeralChange(change_id));

  EXPECT_EQ(std::vector<std::string>({"f:A", "f:B", "f:C"}),
            GetContentFrames(model));
}

TEST(StreamModelTest, RejectEphemeralChange) {
  TestObserver observer;
  StreamModel model(&observer);

  model.ExecuteOperations(MakeTypicalStreamOperations());
  EphemeralChangeId change_id = model.CreateEphemeralChange({
      MakeOperation(MakeCluster("C", MakeRootId())),
      MakeOperation(MakeContentNode("C", MakeClusterId("C"))),
      MakeOperation(MakeContent("C")),
  });
  observer.Clear();

  EXPECT_TRUE(model.RejectEphemeralChange(change_id));
  EXPECT_TRUE(observer.ContentListChanged());
  // Can't commit after reject.
  EXPECT_FALSE(model.CommitEphemeralChange(change_id));

  EXPECT_EQ(std::vector<std::string>({"f:A", "f:B"}), GetContentFrames(model));
}

TEST(StreamModelTest, RejectFirstEphemeralChange) {
  TestObserver observer;
  StreamModel model(&observer);

  model.ExecuteOperations(MakeTypicalStreamOperations());
  EphemeralChangeId change_id1 = model.CreateEphemeralChange({
      MakeOperation(MakeCluster("C", MakeRootId())),
      MakeOperation(MakeContentNode("C", MakeClusterId("C"))),
      MakeOperation(MakeContent("C")),
  });

  model.CreateEphemeralChange({
      MakeOperation(MakeCluster("D", MakeRootId())),
      MakeOperation(MakeContentNode("D", MakeClusterId("D"))),
      MakeOperation(MakeContent("D")),
  });
  observer.Clear();

  EXPECT_TRUE(model.RejectEphemeralChange(change_id1));
  EXPECT_TRUE(observer.ContentListChanged());
  // Can't commit after reject.
  EXPECT_FALSE(model.CommitEphemeralChange(change_id1));

  EXPECT_EQ(std::vector<std::string>({"f:A", "f:B", "f:D"}),
            GetContentFrames(model));
}

}  // namespace
}  // namespace feed
