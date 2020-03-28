// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feed_store.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "components/feed/core/proto/v2/wire/content_id.pb.h"
#include "components/feed/core/v2/test/stream_builder.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

const char kNextPageToken[] = "next page token";
const char kConsistencyToken[] = "consistency token";
const int64_t kLastAddedTimeMs = 100;

feedstore::StreamData MakeStreamData() {
  feedstore::StreamData stream_data;
  *stream_data.mutable_content_id() = MakeRootId();
  stream_data.set_next_page_token(kNextPageToken);
  stream_data.set_consistency_token(kConsistencyToken);
  stream_data.set_last_added_time_millis(kLastAddedTimeMs);

  std::vector<feedstore::DataOperation> operations =
      MakeTypicalStreamOperations();
  for (auto& operation : operations) {
    *stream_data.add_structures() = std::move(operation.structure());
  }

  return stream_data;
}

std::string ContentIdToString(const feedwire::ContentId& content_id) {
  return base::StrCat(
      {"{content_domain: \"", content_id.content_domain(),
       "\", id: ", base::NumberToString(content_id.id()), ", type: \"",
       feedwire::ContentId::Type_Name(content_id.type()), "\"}"});
}

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

const char kRootKey[] = "S/0";

}  // namespace

class FeedStoreTest : public testing::Test {
 public:
  void MakeFeedStore(std::map<std::string, feedstore::Record> entries,
                     leveldb_proto::Enums::InitStatus init_status =
                         leveldb_proto::Enums::InitStatus::kOK) {
    db_entries_ = std::move(entries);
    auto fake_db =
        std::make_unique<leveldb_proto::test::FakeDB<feedstore::Record>>(
            &db_entries_);
    fake_db_ = fake_db.get();
    store_ = std::make_unique<FeedStore>(std::move(fake_db));
    fake_db_->InitStatusCallback(init_status);
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
  EXPECT_FALSE(store->IsInitializedForTesting());

  fake_db_raw->InitStatusCallback(leveldb_proto::Enums::InitStatus::kError);
  EXPECT_FALSE(store->IsInitializedForTesting());
}

TEST_F(FeedStoreTest, ReadStreamData) {
  feedstore::Record record;
  *record.mutable_stream_data() = MakeStreamData();
  MakeFeedStore({{kRootKey, record}});

  // Successful read
  bool did_successful_read = false;
  store_->ReadStreamData(base::BindLambdaForTesting(
      [&](std::unique_ptr<feedstore::StreamData> stream_data) {
        did_successful_read = true;
        ASSERT_TRUE(stream_data);
        EXPECT_EQ(ContentIdToString(stream_data->content_id()),
                  ContentIdToString(record.stream_data().content_id()));
        EXPECT_EQ(stream_data->structures_size(),
                  record.stream_data().structures_size());
        EXPECT_EQ(stream_data->next_page_token(),
                  record.stream_data().next_page_token());
        EXPECT_EQ(stream_data->consistency_token(),
                  record.stream_data().consistency_token());
        EXPECT_EQ(stream_data->last_added_time_millis(),
                  record.stream_data().last_added_time_millis());
        EXPECT_EQ(stream_data->next_action_id(),
                  record.stream_data().next_action_id());
      }));
  fake_db_->GetCallback(true);
  EXPECT_TRUE(did_successful_read);

  // Failed read
  bool did_failed_read = false;
  store_->ReadStreamData(base::BindLambdaForTesting(
      [&](std::unique_ptr<feedstore::StreamData> stream_data) {
        did_failed_read = true;
        EXPECT_FALSE(stream_data);
      }));
  fake_db_->GetCallback(false);
  EXPECT_TRUE(did_failed_read);
}

TEST_F(FeedStoreTest, ReadNonexistentStreamData) {
  MakeFeedStore({});

  bool did_read = false;
  store_->ReadStreamData(base::BindLambdaForTesting(
      [&](std::unique_ptr<feedstore::StreamData> stream_data) {
        did_read = true;
        EXPECT_FALSE(stream_data);
      }));
  fake_db_->GetCallback(true);
  EXPECT_TRUE(did_read);
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
            EXPECT_EQ(ContentIdToString(content[0].content_id()),
                      ContentIdToString(content1.content_id()));
            EXPECT_EQ(content[0].frame(), content1.frame());

            ASSERT_EQ(shared_states.size(), 2ul);
            EXPECT_EQ(ContentIdToString(shared_states[0].content_id()),
                      ContentIdToString(shared1.content_id()));
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

TEST_F(FeedStoreTest, ReadNextStreamState) {
  feedstore::Record record;
  feedstore::StreamAndContentState* next_stream_state =
      record.mutable_next_stream_state();
  *next_stream_state->mutable_stream_data() = MakeStreamData();
  *next_stream_state->add_content() = MakeContent(0);
  *next_stream_state->add_shared_state() = MakeSharedState(0);

  MakeFeedStore({{"N", record}});

  // Successful read
  bool did_successful_read = false;
  store_->ReadNextStreamState(base::BindLambdaForTesting(
      [&](std::unique_ptr<feedstore::StreamAndContentState> result) {
        did_successful_read = true;
        ASSERT_TRUE(result);
        EXPECT_TRUE(result->has_stream_data());
        EXPECT_EQ(result->content_size(), 1);
        EXPECT_EQ(result->shared_state_size(), 1);
      }));
  fake_db_->GetCallback(true);
  EXPECT_TRUE(did_successful_read);

  // Failed read
  bool did_failed_read = false;
  store_->ReadNextStreamState(base::BindLambdaForTesting(
      [&](std::unique_ptr<feedstore::StreamAndContentState> result) {
        did_failed_read = true;
        EXPECT_FALSE(result);
      }));
  fake_db_->GetCallback(false);
  EXPECT_TRUE(did_failed_read);
}

TEST_F(FeedStoreTest, WriteThenRead) {
  MakeFeedStore({});

  std::vector<feedstore::Record> records(4);
  *records[0].mutable_stream_data() = MakeStreamData();
  *records[1].mutable_content() = MakeContent(0);
  *records[2].mutable_shared_state() = MakeSharedState(0);
  *records[3].mutable_next_stream_state()->mutable_stream_data() =
      MakeStreamData();

  store_->Write(records, base::BindLambdaForTesting([](bool success) {}));
  fake_db_->UpdateCallback(true);

  bool did_read_stream_data = false;
  store_->ReadStreamData(base::BindLambdaForTesting(
      [&](std::unique_ptr<feedstore::StreamData> stream_data) {
        did_read_stream_data = true;
        ASSERT_TRUE(stream_data);
        // Just make sure stream_data isn't a default empty StreamData.
        EXPECT_TRUE(stream_data->has_content_id());
      }));
  fake_db_->GetCallback(true);
  EXPECT_TRUE(did_read_stream_data);

  bool did_read_content = false;
  store_->ReadContent(
      {records[1].content().content_id()},
      {records[2].shared_state().content_id()},
      base::BindLambdaForTesting(
          [&](std::vector<feedstore::Content> content,
              std::vector<feedstore::StreamSharedState> shared_states) {
            did_read_content = true;
            ASSERT_EQ(content.size(), 1ul);
            EXPECT_TRUE(content[0].has_content_id());

            ASSERT_EQ(shared_states.size(), 1ul);
            EXPECT_TRUE(shared_states[0].has_content_id());
          }));
  fake_db_->LoadCallback(true);
  EXPECT_TRUE(did_read_content);

  bool did_read_next_stream_state = false;
  store_->ReadNextStreamState(base::BindLambdaForTesting(
      [&](std::unique_ptr<feedstore::StreamAndContentState> result) {
        did_read_next_stream_state = true;
        ASSERT_TRUE(result);
        EXPECT_TRUE(result->has_stream_data());
      }));
  fake_db_->GetCallback(true);
  EXPECT_TRUE(did_read_next_stream_state);
}

}  // namespace feed
