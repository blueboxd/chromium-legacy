// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/protocol_translator.h"

#include <sstream>
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
#include "components/feed/core/v2/test/proto_printer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

const char kResponsePbPath[] = "components/test/data/feed/response.binarypb";
const base::Time kCurrentTime =
    base::Time::UnixEpoch() + base::TimeDelta::FromDays(123);

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

  RefreshResponseData translated = TranslateWireResponse(
      response, StreamModelUpdateRequest::Source::kNetworkUpdate, kCurrentTime);

  ASSERT_TRUE(translated.model_update_request);
  ASSERT_TRUE(translated.request_schedule);
  EXPECT_EQ(kCurrentTime, translated.request_schedule->anchor_time);
  EXPECT_EQ(std::vector<base::TimeDelta>(
                {base::TimeDelta::FromSeconds(86308) +
                 base::TimeDelta::FromNanoseconds(822963644)}),
            translated.request_schedule->refresh_offsets);

  std::stringstream ss;
  ss << *translated.model_update_request;

  const std::string want = R"(source: 0
stream_data: {
  last_added_time_millis: 10627200000
  shared_state_id {
    content_domain: "render_data"
  }
}
content: {
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 3328940074512586021
  }
  frame: "data2"
}
content: {
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 8191455549164721606
  }
  frame: "data3"
}
content: {
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 10337142060535577025
  }
  frame: "data4"
}
content: {
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 9467333465122011616
  }
  frame: "data5"
}
content: {
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 10024917518268143371
  }
  frame: "data6"
}
content: {
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 14956621708214864803
  }
  frame: "data7"
}
content: {
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 2741853109953412745
  }
  frame: "data8"
}
content: {
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 586433679892097787
  }
  frame: "data9"
}
content: {
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 790985792726953756
  }
  frame: "data10"
}
content: {
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 7324025093440047528
  }
  frame: "data11"
}
shared_state: {
  content_id {
    content_domain: "render_data"
  }
  shared_state_data: "data1"
}
stream_structure: {
  operation: 1
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "root"
  }
  type: 1
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "render_data"
  }
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 3328940074512586021
  }
  parent_id {
    content_domain: "content.f"
    type: 3
    id: 14679492703605464401
  }
  type: 3
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "content.f"
    type: 3
    id: 14679492703605464401
  }
  parent_id {
    content_domain: "root"
  }
  type: 4
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 8191455549164721606
  }
  parent_id {
    content_domain: "content.f"
    type: 3
    id: 16663153735812675251
  }
  type: 3
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "content.f"
    type: 3
    id: 16663153735812675251
  }
  parent_id {
    content_domain: "root"
  }
  type: 4
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 10337142060535577025
  }
  parent_id {
    content_domain: "content.f"
    type: 3
    id: 15532023010474785878
  }
  type: 3
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "content.f"
    type: 3
    id: 15532023010474785878
  }
  parent_id {
    content_domain: "root"
  }
  type: 4
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 9467333465122011616
  }
  parent_id {
    content_domain: "content.f"
    type: 3
    id: 10111267591181086437
  }
  type: 3
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "content.f"
    type: 3
    id: 10111267591181086437
  }
  parent_id {
    content_domain: "root"
  }
  type: 4
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 10024917518268143371
  }
  parent_id {
    content_domain: "content.f"
    type: 3
    id: 6703713839373923610
  }
  type: 3
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "content.f"
    type: 3
    id: 6703713839373923610
  }
  parent_id {
    content_domain: "root"
  }
  type: 4
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 14956621708214864803
  }
  parent_id {
    content_domain: "content.f"
    type: 3
    id: 12592500096310265284
  }
  type: 3
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "content.f"
    type: 3
    id: 12592500096310265284
  }
  parent_id {
    content_domain: "root"
  }
  type: 4
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 2741853109953412745
  }
  parent_id {
    content_domain: "content.f"
    type: 3
    id: 1016582787945881825
  }
  type: 3
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "content.f"
    type: 3
    id: 1016582787945881825
  }
  parent_id {
    content_domain: "root"
  }
  type: 4
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 586433679892097787
  }
  parent_id {
    content_domain: "content.f"
    type: 3
    id: 9506447424580769257
  }
  type: 3
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "content.f"
    type: 3
    id: 9506447424580769257
  }
  parent_id {
    content_domain: "root"
  }
  type: 4
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 790985792726953756
  }
  parent_id {
    content_domain: "content.f"
    type: 3
    id: 17612738377810195843
  }
  type: 3
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "content.f"
    type: 3
    id: 17612738377810195843
  }
  parent_id {
    content_domain: "root"
  }
  type: 4
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 7324025093440047528
  }
  parent_id {
    content_domain: "content.f"
    type: 3
    id: 5093490247022575399
  }
  type: 3
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "content.f"
    type: 3
    id: 5093490247022575399
  }
  parent_id {
    content_domain: "root"
  }
  type: 4
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "request_schedule"
    id: 300842786
  }
}
max_structure_sequence_number: 0
)";
  EXPECT_EQ(want, ss.str());
}

}  // namespace feed
