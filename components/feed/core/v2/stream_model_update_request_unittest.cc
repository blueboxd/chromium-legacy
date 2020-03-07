// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/stream_model_update_request.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/feed/core/proto/libraries/api/internal/stream_data.pb.h"
#include "components/feed/core/proto/wire/feed_response.pb.h"
#include "components/feed/core/proto/wire/response.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

const char kResponsePbPath[] = "components/test/data/feed/response.binarypb";
constexpr base::TimeDelta kResponseTime = base::TimeDelta::FromSeconds(42);

const int kExpectedStreamStructureCount = 34;
const size_t kExpectedContentCount = 10;
const size_t kExpectedSharedStateCount = 1;

std::string ContentIdToString(const feedwire::ContentId& content_id) {
  return "{content_domain: \"" + content_id.content_domain() +
         "\", id: " + base::NumberToString(content_id.id()) + ", table: \"" +
         content_id.table() + "\"}";
}

feedwire::Response TestWireResponse() {
  // Read and parse response.binarypb.
  base::FilePath response_file_path;
  DCHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &response_file_path));
  response_file_path = response_file_path.AppendASCII(kResponsePbPath);

  DCHECK(base::PathExists(response_file_path))
      << "Path doesn't exist: " << response_file_path;

  std::string response_data;
  DCHECK(base::ReadFileToString(response_file_path, &response_data));

  feedwire::Response response;
  DCHECK(response.ParseFromString(response_data));
  return response;
}

}  // namespace

// TODO(iwells): Test failure cases once the new protos are ready.

TEST(StreamModelUpdateRequestTest, TranslateRealResponse) {
  // Tests how proto translation works on a real response from the server.
  //
  // The response will periodically need to be updated as changes are made to
  // the server. Update testdata/response.textproto and then run
  // tools/generate_test_response_binarypb.sh.

  feedwire::Response response = TestWireResponse();
  feedwire::FeedResponse feed_response =
      response.GetExtension(feedwire::FeedResponse::feed_response);
  ASSERT_EQ(feed_response.data_operation_size(), kExpectedStreamStructureCount);

  std::unique_ptr<StreamModelUpdateRequest> translated =
      TranslateWireResponse(response, kResponseTime);

  ASSERT_TRUE(translated);
  ASSERT_EQ(translated->stream_data.structures_size(),
            kExpectedStreamStructureCount);

  const google::protobuf::RepeatedPtrField<feedstore::StreamStructure>&
      structures = translated->stream_data.structures();

  // Check CLEAR_ALL:
  EXPECT_EQ(structures[0].operation(), feedstore::StreamStructure::CLEAR_ALL);

  // Check UPDATE_OR_APPEND for a shared state:
  EXPECT_EQ(structures[1].operation(),
            feedstore::StreamStructure::UPDATE_OR_APPEND);
  EXPECT_EQ(structures[1].type(), feedstore::StreamStructure::UNKNOWN_TYPE);
  EXPECT_TRUE(structures[1].has_content_id());
  ASSERT_TRUE(translated->shared_states.size() > 0);
  EXPECT_EQ(ContentIdToString(translated->shared_states[0].content_id()),
            ContentIdToString(structures[1].content_id()));
  // TODO(iwells): More checks on shared_state here

  // Check UPDATE_OR_APPEND for the root:
  EXPECT_EQ(structures[2].operation(),
            feedstore::StreamStructure::UPDATE_OR_APPEND);
  EXPECT_EQ(structures[2].type(), feedstore::StreamStructure::STREAM);
  EXPECT_TRUE(structures[2].has_content_id());
  EXPECT_FALSE(structures[2].has_parent_id());

  feedwire::ContentId root_content_id = structures[2].content_id();

  // Content:
  EXPECT_EQ(structures[3].operation(),
            feedstore::StreamStructure::UPDATE_OR_APPEND);
  EXPECT_EQ(structures[3].type(), feedstore::StreamStructure::CONTENT);
  EXPECT_TRUE(structures[3].has_content_id());
  EXPECT_TRUE(structures[3].has_parent_id());
  EXPECT_TRUE(structures[3].has_content_info());
  EXPECT_NE(structures[3].content_info().score(), 0.);
  EXPECT_NE(structures[3].content_info().availability_time_seconds(), 0);
  EXPECT_TRUE(structures[3].content_info().has_representation_data());
  EXPECT_TRUE(structures[3].content_info().has_offline_metadata());

  ASSERT_TRUE(translated->content.size() > 0);
  EXPECT_EQ(ContentIdToString(translated->content[0].content_id()),
            ContentIdToString(structures[3].content_id()));
  // TODO: Check content.frame() once this is available.

  // Non-content structures:
  EXPECT_EQ(structures[4].operation(),
            feedstore::StreamStructure::UPDATE_OR_APPEND);
  EXPECT_EQ(structures[4].type(), feedstore::StreamStructure::CARD);
  EXPECT_TRUE(structures[4].has_content_id());
  EXPECT_TRUE(structures[4].has_parent_id());

  EXPECT_EQ(structures[5].operation(),
            feedstore::StreamStructure::UPDATE_OR_APPEND);
  EXPECT_EQ(structures[5].type(), feedstore::StreamStructure::CLUSTER);
  EXPECT_TRUE(structures[5].has_content_id());
  EXPECT_TRUE(structures[5].has_parent_id());
  EXPECT_EQ(ContentIdToString(structures[5].parent_id()),
            ContentIdToString(root_content_id));

  // The other members:
  EXPECT_EQ(translated->content.size(), kExpectedContentCount);
  EXPECT_EQ(translated->shared_states.size(), kExpectedSharedStateCount);

  EXPECT_EQ(translated->response_time, kResponseTime);
}

}  // namespace feed
