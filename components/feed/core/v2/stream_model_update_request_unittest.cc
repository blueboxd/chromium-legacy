// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/stream_model_update_request.h"

#include <string>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/wire/feed_response.pb.h"
#include "components/feed/core/proto/v2/wire/response.pb.h"
#include "components/feed/core/v2/proto_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

const char kResponsePbPath[] = "components/test/data/feed/response.binarypb";
constexpr base::TimeDelta kResponseTime = base::TimeDelta::FromSeconds(42);
const base::Time kCurrentTime =
    base::Time::UnixEpoch() + base::TimeDelta::FromDays(123);
// TODO(iwells): Replace response.binarypb with a response that uses the new
// wire protocol.
//
// Since we're currently using a Jardin response which includes a
// Piet shared state, and translation skips handling Piet shared states, we
// expect to have only 33 StreamStructures even though there are 34 wire
// operations.
const int kExpectedStreamStructureCount = 33;
const size_t kExpectedContentCount = 10;
const size_t kExpectedSharedStateCount = 0;

std::string ContentIdToString(const feedwire::ContentId& content_id) {
  return "{content_domain: \"" + content_id.content_domain() +
         "\", id: " + base::NumberToString(content_id.id()) + ", type: \"" +
         feedwire::ContentId::Type_Name(content_id.type()) + "\"}";
}

feedwire::Response TestWireResponse() {
  // Read and parse response.binarypb.
  base::FilePath response_file_path;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &response_file_path));
  response_file_path = response_file_path.AppendASCII(kResponsePbPath);

  CHECK(base::PathExists(response_file_path));

  std::string response_data;
  CHECK(base::ReadFileToString(response_file_path, &response_data));

  feedwire::Response response;
  CHECK(response.ParseFromString(response_data));
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
  feedwire::FeedResponse feed_response = response.feed_response();

  // TODO(iwells): Make these exactly equal once we aren't using an old
  // response.
  ASSERT_EQ(feed_response.data_operation_size(),
            kExpectedStreamStructureCount + 1);

  std::unique_ptr<StreamModelUpdateRequest> translated =
      TranslateWireResponse(response, kResponseTime, kCurrentTime);

  ASSERT_TRUE(translated);
  EXPECT_EQ(kCurrentTime, feedstore::GetLastAddedTime(translated->stream_data));
  ASSERT_EQ(translated->stream_structures.size(),
            static_cast<size_t>(kExpectedStreamStructureCount));

  const std::vector<feedstore::StreamStructure>& structures =
      translated->stream_structures;

  // Check CLEAR_ALL:
  EXPECT_EQ(structures[0].operation(), feedstore::StreamStructure::CLEAR_ALL);

  // TODO(iwells): Check the shared state once we have a new

  // Check UPDATE_OR_APPEND for the root:
  EXPECT_EQ(structures[1].operation(),
            feedstore::StreamStructure::UPDATE_OR_APPEND);
  EXPECT_EQ(structures[1].type(), feedstore::StreamStructure::STREAM);
  EXPECT_TRUE(structures[1].has_content_id());
  EXPECT_FALSE(structures[1].has_parent_id());

  feedwire::ContentId root_content_id = structures[1].content_id();

  // Content:
  EXPECT_EQ(structures[2].operation(),
            feedstore::StreamStructure::UPDATE_OR_APPEND);
  EXPECT_EQ(structures[2].type(), feedstore::StreamStructure::CONTENT);
  EXPECT_TRUE(structures[2].has_content_id());
  EXPECT_TRUE(structures[2].has_parent_id());

  // TODO(iwells): Uncomment when these are available.
  // EXPECT_TRUE(structures[3].has_content_info());
  // EXPECT_NE(structures[3].content_info().score(), 0.);
  // EXPECT_NE(structures[3].content_info().availability_time_seconds(), 0);
  // EXPECT_TRUE(structures[3].content_info().has_representation_data());
  // EXPECT_TRUE(structures[3].content_info().has_offline_metadata());

  ASSERT_GT(translated->content.size(), 0UL);
  EXPECT_EQ(ContentIdToString(translated->content[0].content_id()),
            ContentIdToString(structures[2].content_id()));
  // TODO(iwells): Check content.frame() once this is available.

  // Non-content structures:
  EXPECT_EQ(structures[3].operation(),
            feedstore::StreamStructure::UPDATE_OR_APPEND);
  // TODO(iwells): This is a CARD. Remove once we have a new response.
  EXPECT_EQ(structures[3].type(), feedstore::StreamStructure::UNKNOWN_TYPE);
  EXPECT_TRUE(structures[3].has_content_id());
  EXPECT_TRUE(structures[3].has_parent_id());

  EXPECT_EQ(structures[4].operation(),
            feedstore::StreamStructure::UPDATE_OR_APPEND);
  EXPECT_EQ(structures[4].type(), feedstore::StreamStructure::CLUSTER);
  EXPECT_TRUE(structures[4].has_content_id());
  EXPECT_TRUE(structures[4].has_parent_id());
  EXPECT_EQ(ContentIdToString(structures[4].parent_id()),
            ContentIdToString(root_content_id));

  // The other members:
  EXPECT_EQ(translated->content.size(), kExpectedContentCount);
  EXPECT_EQ(translated->shared_states.size(), kExpectedSharedStateCount);

  EXPECT_EQ(translated->response_time, kResponseTime);
}

}  // namespace feed
