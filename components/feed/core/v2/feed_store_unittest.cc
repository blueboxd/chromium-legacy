// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feed_store.h"

#include <map>
#include <set>
#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "components/feed/core/proto/v2/wire/content_id.pb.h"
#include "components/feed/core/v2/protocol_translator.h"
#include "components/feed/core/v2/test/callback_receiver.h"
#include "components/feed/core/v2/test/proto_printer.h"
#include "components/feed/core/v2/test/stream_builder.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

using LoadStreamResult = FeedStore::LoadStreamResult;

std::string KeyForContentId(base::StringPiece prefix,
                            const feedwire::ContentId& content_id) {
  return base::StrCat({prefix, content_id.content_domain(), ",",
                       base::NumberToString(content_id.type()), ",",
                       base::NumberToString(content_id.id())});
}

feedstore::Record RecordForContent(feedstore::Content content) {
  feedstore::Record record;
  *record.mutable_content() = std::move(content);
  return record;
}

feedstore::Record RecordForSharedState(feedstore::StreamSharedState shared) {
  feedstore::Record record;
  *record.mutable_shared_state() = std::move(shared);
  return record;
}

feedstore::Record RecordForAction(feedstore::StoredAction action) {
  feedstore::Record record;
  *record.mutable_local_action() = std::move(action);
  return record;
}

feedstore::StoredAction MakeAction(int32_t id) {
  feedstore::StoredAction action;
  action.set_id(id);
  return action;
}

}  // namespace

class FeedStoreTest : public testing::Test {
 protected:
  void MakeFeedStore(std::map<std::string, feedstore::Record> entries,
                     leveldb_proto::Enums::InitStatus init_status =
                         leveldb_proto::Enums::InitStatus::kOK) {
    db_entries_ = std::move(entries);
    auto fake_db =
        std::make_unique<leveldb_proto::test::FakeDB<feedstore::Record>>(
            &db_entries_);
    fake_db_ = fake_db.get();
    store_ = std::make_unique<FeedStore>(std::move(fake_db));
    store_->Initialize(base::DoNothing());
    fake_db_->InitStatusCallback(init_status);
  }

  std::set<std::string> StoredKeys() {
    std::set<std::string> result;
    for (auto& entry : db_entries_) {
      result.insert(entry.first);
    }
    return result;
  }

  std::string StoreToString() {
    std::stringstream ss;
    for (auto& entry : db_entries_) {
      ss << "[" << entry.first << "] " << entry.second;
    }
    return ss.str();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::SYSTEM_TIME};
  std::unique_ptr<FeedStore> store_;
  std::map<std::string, feedstore::Record> db_entries_;
  leveldb_proto::test::FakeDB<feedstore::Record>* fake_db_;
};

TEST_F(FeedStoreTest, InitSuccess) {
  MakeFeedStore({});
  EXPECT_TRUE(store_->IsInitializedForTesting());
}

TEST_F(FeedStoreTest, InitFailure) {
  std::map<std::string, feedstore::Record> entries;
  auto fake_db =
      std::make_unique<leveldb_proto::test::FakeDB<feedstore::Record>>(
          &entries);
  leveldb_proto::test::FakeDB<feedstore::Record>* fake_db_raw = fake_db.get();
  auto store = std::make_unique<FeedStore>(std::move(fake_db));

  store->Initialize(base::DoNothing());
  EXPECT_FALSE(store->IsInitializedForTesting());

  fake_db_raw->InitStatusCallback(leveldb_proto::Enums::InitStatus::kError);
  EXPECT_FALSE(store->IsInitializedForTesting());
}

TEST_F(FeedStoreTest, OverwriteStream) {
  MakeFeedStore({});
  CallbackReceiver<bool> receiver;
  store_->OverwriteStream(MakeTypicalInitialModelState(), receiver.Bind());
  fake_db_->UpdateCallback(true);

  ASSERT_TRUE(receiver.GetResult());

  EXPECT_EQ(StoreToString(), R"([S/0] {
  stream_data {
    content_id {
      content_domain: "root"
    }
    next_page_token: "page-2"
    consistency_token: "token-1"
    shared_state_id {
      content_domain: "render_data"
    }
  }
}
[T/0/0] {
  stream_structures {
    stream_id: "0"
    structures {
      operation: 1
    }
    structures {
      operation: 2
      content_id {
        content_domain: "root"
      }
      type: 1
    }
    structures {
      operation: 2
      content_id {
        content_domain: "content"
        type: 3
      }
      parent_id {
        content_domain: "root"
      }
      type: 4
    }
    structures {
      operation: 2
      content_id {
        content_domain: "stories"
        type: 4
      }
      parent_id {
        content_domain: "content"
        type: 3
      }
      type: 3
    }
    structures {
      operation: 2
      content_id {
        content_domain: "content"
        type: 3
        id: 1
      }
      parent_id {
        content_domain: "root"
      }
      type: 4
    }
    structures {
      operation: 2
      content_id {
        content_domain: "stories"
        type: 4
        id: 1
      }
      parent_id {
        content_domain: "content"
        type: 3
        id: 1
      }
      type: 3
    }
  }
}
[c/stories,4,0] {
  content {
    content_id {
      content_domain: "stories"
      type: 4
    }
    frame: "f:0"
  }
}
[c/stories,4,1] {
  content {
    content_id {
      content_domain: "stories"
      type: 4
      id: 1
    }
    frame: "f:1"
  }
}
[s/render_data,0,0] {
  shared_state {
    content_id {
      content_domain: "render_data"
    }
    shared_state_data: "ss:0"
  }
}
)");
}

TEST_F(FeedStoreTest, OverwriteStreamOverwritesData) {
  MakeFeedStore({});
  // Insert some junk that should be removed.
  db_entries_["S/0"].mutable_local_action()->set_id(6);
  db_entries_["T/0/0"].mutable_local_action()->set_id(6);
  db_entries_["T/0/73"].mutable_local_action()->set_id(6);
  db_entries_["c/stories,4,0"].mutable_local_action()->set_id(6);
  db_entries_["c/stories,4,1"].mutable_local_action()->set_id(6);
  db_entries_["c/garbage"].mutable_local_action()->set_id(6);
  db_entries_["s/render_data,0,0"].mutable_local_action()->set_id(6);
  db_entries_["s/garbage,0,0"].mutable_local_action()->set_id(6);

  CallbackReceiver<bool> receiver;
  store_->OverwriteStream(MakeTypicalInitialModelState(), receiver.Bind());
  fake_db_->UpdateCallback(true);

  ASSERT_TRUE(receiver.GetResult());
  ASSERT_EQ(std::set<std::string>({
                "S/0",
                "T/0/0",
                "c/stories,4,0",
                "c/stories,4,1",
                "s/render_data,0,0",
            }),
            StoredKeys());

  for (std::string key : StoredKeys()) {
    EXPECT_FALSE(db_entries_[key].has_local_action())
        << "Found local action at key " << key
        << ", did OverwriteStream erase everything?";
  }
}

TEST_F(FeedStoreTest, LoadStreamSuccess) {
  MakeFeedStore({});
  store_->OverwriteStream(MakeTypicalInitialModelState(), base::DoNothing());
  fake_db_->UpdateCallback(true);

  CallbackReceiver<LoadStreamResult> receiver;
  store_->LoadStream(receiver.Bind());
  fake_db_->LoadCallback(true);

  ASSERT_TRUE(receiver.GetResult());
  EXPECT_FALSE(receiver.GetResult()->read_error);
  EXPECT_EQ(ToTextProto(MakeRootId()),
            ToTextProto(receiver.GetResult()->stream_data.content_id()));
}

TEST_F(FeedStoreTest, LoadStreamFail) {
  MakeFeedStore({});
  store_->OverwriteStream(MakeTypicalInitialModelState(), base::DoNothing());
  fake_db_->UpdateCallback(true);

  CallbackReceiver<LoadStreamResult> receiver;
  store_->LoadStream(receiver.Bind());
  fake_db_->LoadCallback(false);

  ASSERT_TRUE(receiver.GetResult());
  EXPECT_TRUE(receiver.GetResult()->read_error);
}

TEST_F(FeedStoreTest, LoadStreamNoData) {
  MakeFeedStore({});

  CallbackReceiver<LoadStreamResult> receiver;
  store_->LoadStream(receiver.Bind());
  fake_db_->LoadCallback(true);

  ASSERT_TRUE(receiver.GetResult());
  EXPECT_FALSE(receiver.GetResult()->stream_data.has_content_id());
}

TEST_F(FeedStoreTest, WriteOperations) {
  MakeFeedStore({});
  CallbackReceiver<LoadStreamResult> receiver;
  store_->WriteOperations(5, {MakeOperation(MakeCluster(2, MakeRootId())),
                              MakeOperation(MakeCluster(6, MakeRootId()))});
  fake_db_->UpdateCallback(true);

  EXPECT_EQ(StoreToString(), R"([T/0/5] {
  stream_structures {
    stream_id: "0"
    sequence_number: 5
    structures {
      operation: 2
      content_id {
        content_domain: "content"
        type: 3
        id: 2
      }
      parent_id {
        content_domain: "root"
      }
      type: 4
    }
    structures {
      operation: 2
      content_id {
        content_domain: "content"
        type: 3
        id: 6
      }
      parent_id {
        content_domain: "root"
      }
      type: 4
    }
  }
}
)");
}

TEST_F(FeedStoreTest, ReadNonexistentContentAndSharedStates) {
  MakeFeedStore({});

  bool did_read = false;
  store_->ReadContent(
      {MakeContentContentId(0)}, {MakeSharedStateContentId(0)},
      base::BindLambdaForTesting(
          [&](std::vector<feedstore::Content> content,
              std::vector<feedstore::StreamSharedState> shared_states) {
            did_read = true;
            EXPECT_EQ(content.size(), 0ul);
            EXPECT_EQ(shared_states.size(), 0ul);
          }));
  fake_db_->LoadCallback(true);
  EXPECT_TRUE(did_read);
}

TEST_F(FeedStoreTest, ReadContentAndSharedStates) {
  feedstore::Content content1 = MakeContent(1);
  feedstore::Content content2 = MakeContent(2);
  feedstore::StreamSharedState shared1 = MakeSharedState(1);
  feedstore::StreamSharedState shared2 = MakeSharedState(2);

  MakeFeedStore({{KeyForContentId("c/", content1.content_id()),
                  RecordForContent(content1)},
                 {KeyForContentId("c/", content2.content_id()),
                  RecordForContent(content2)},
                 {KeyForContentId("s/", shared1.content_id()),
                  RecordForSharedState(shared1)},
                 {KeyForContentId("s/", shared2.content_id()),
                  RecordForSharedState(shared2)}});

  std::vector<feedwire::ContentId> content_ids = {content1.content_id(),
                                                  content2.content_id()};
  std::vector<feedwire::ContentId> shared_state_ids = {shared1.content_id(),
                                                       shared2.content_id()};

  // Successful read
  bool did_successful_read = false;
  store_->ReadContent(
      content_ids, shared_state_ids,
      base::BindLambdaForTesting(
          [&](std::vector<feedstore::Content> content,
              std::vector<feedstore::StreamSharedState> shared_states) {
            did_successful_read = true;
            ASSERT_EQ(content.size(), 2ul);
            EXPECT_EQ(ToTextProto(content[0].content_id()),
                      ToTextProto(content1.content_id()));
            EXPECT_EQ(content[0].frame(), content1.frame());

            ASSERT_EQ(shared_states.size(), 2ul);
            EXPECT_EQ(ToTextProto(shared_states[0].content_id()),
                      ToTextProto(shared1.content_id()));
            EXPECT_EQ(shared_states[0].shared_state_data(),
                      shared1.shared_state_data());
          }));
  fake_db_->LoadCallback(true);
  EXPECT_TRUE(did_successful_read);

  // Failed read
  bool did_failed_read = false;
  store_->ReadContent(
      content_ids, shared_state_ids,
      base::BindLambdaForTesting(
          [&](std::vector<feedstore::Content> content,
              std::vector<feedstore::StreamSharedState> shared_states) {
            did_failed_read = true;
            EXPECT_EQ(content.size(), 0ul);
            EXPECT_EQ(shared_states.size(), 0ul);
          }));
  fake_db_->LoadCallback(false);
  EXPECT_TRUE(did_failed_read);
}

TEST_F(FeedStoreTest, ReadActions) {
  feedstore::StoredAction action0 = MakeAction(0);
  feedstore::StoredAction action1 = MakeAction(1);
  feedstore::StoredAction action2 = MakeAction(2);
  MakeFeedStore({{"a/0", RecordForAction(action0)},
                 {"a/1", RecordForAction(action1)},
                 {"a/2", RecordForAction(action2)}});

  // Successful read
  CallbackReceiver<std::vector<feedstore::StoredAction>> receiver;
  store_->ReadActions(receiver.Bind());
  fake_db_->LoadCallback(true);
  EXPECT_NE(receiver.GetResult(), base::nullopt);
  EXPECT_EQ(receiver.GetResult()->size(), 3ul);

  // Failed read
  receiver.GetResult().reset();
  store_->ReadActions(receiver.Bind());
  fake_db_->LoadCallback(false);
  EXPECT_NE(receiver.GetResult(), base::nullopt);
  EXPECT_EQ(receiver.GetResult()->size(), 0ul);
}

TEST_F(FeedStoreTest, WriteActions) {
  MakeFeedStore({});
  feedstore::StoredAction action = MakeAction(5);

  CallbackReceiver<bool> receiver;
  store_->WriteActions({action}, receiver.Bind());
  fake_db_->UpdateCallback(true);
  EXPECT_EQ(receiver.GetResult().value(), true);
  ASSERT_EQ(db_entries_.size(), 1ul);
  EXPECT_EQ(db_entries_["a/5"].local_action().id(), 5);

  receiver.GetResult().reset();
  store_->WriteActions({action}, receiver.Bind());
  fake_db_->UpdateCallback(false);
  EXPECT_NE(receiver.GetResult(), base::nullopt);
  EXPECT_EQ(receiver.GetResult().value(), false);
}

TEST_F(FeedStoreTest, RemoveActions) {
  feedstore::StoredAction action0 = MakeAction(0);
  feedstore::StoredAction action1 = MakeAction(1);
  feedstore::StoredAction action2 = MakeAction(2);
  MakeFeedStore({{"a/0", RecordForAction(action0)},
                 {"a/1", RecordForAction(action1)},
                 {"a/2", RecordForAction(action2)}});

  const std::vector<LocalActionId> ids = {LocalActionId(0), LocalActionId(1),
                                          LocalActionId(2)};

  CallbackReceiver<bool> receiver;
  store_->RemoveActions(ids, receiver.Bind());
  fake_db_->UpdateCallback(true);
  EXPECT_EQ(receiver.GetResult().value(), true);
  EXPECT_EQ(db_entries_.size(), 0ul);

  receiver.GetResult().reset();
  store_->RemoveActions(ids, receiver.Bind());
  fake_db_->UpdateCallback(false);
  EXPECT_NE(receiver.GetResult(), base::nullopt);
  EXPECT_EQ(receiver.GetResult().value(), false);
}

TEST_F(FeedStoreTest, ClearAllSuccess) {
  // Write at least one record of each type.
  MakeFeedStore({});
  store_->OverwriteStream(MakeTypicalInitialModelState(), base::DoNothing());
  fake_db_->UpdateCallback(true);
  store_->WriteActions({MakeAction(0)}, base::DoNothing());
  fake_db_->UpdateCallback(true);
  ASSERT_NE("", StoreToString());

  // ClearAll() and verify the DB is empty.
  CallbackReceiver<bool> receiver;
  store_->ClearAll(receiver.Bind());
  fake_db_->UpdateCallback(true);

  ASSERT_TRUE(receiver.GetResult());
  EXPECT_TRUE(*receiver.GetResult());
  EXPECT_EQ("", StoreToString());
}

TEST_F(FeedStoreTest, ClearAllFail) {
  // Just verify that we can handle a storage failure. Note that |FakeDB| will
  // actually perform operations even when UpdateCallback(false) is called.
  MakeFeedStore({});

  CallbackReceiver<bool> receiver;
  store_->ClearAll(receiver.Bind());
  fake_db_->UpdateCallback(false);

  ASSERT_TRUE(receiver.GetResult());
  EXPECT_FALSE(*receiver.GetResult());
}

}  // namespace feed
