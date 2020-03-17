// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_TEST_STREAM_BUILDER_H_
#define COMPONENTS_FEED_CORE_V2_TEST_STREAM_BUILDER_H_

#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/v2/public/feed_stream_api.h"

// Functions that help build a feedstore::StreamStructure for testing.
namespace feed {
ContentId MakeContentId(const std::string& domain, int id_number);
ContentId MakeClusterId(int id_number);
ContentId MakeContentContentId(int id_number);
ContentId MakeRootId(int id_number = 0);
feedstore::StreamStructure MakeStream(int id_number = 0);
feedstore::StreamStructure MakeCluster(int id_number, ContentId parent);
feedstore::StreamStructure MakeContentNode(int id_number, ContentId parent);
feedstore::StreamStructure MakeRemove(ContentId id);
feedstore::Content MakeContent(int id_number);
feedstore::DataOperation MakeOperation(feedstore::StreamStructure structure);
feedstore::DataOperation MakeOperation(feedstore::Content content);

// Returns data operations to create a stypical stream:
// Root
// |-Cluster 0
// |  |-Content 0
// |-Cluster 1
//    |-Content 1
std::vector<feedstore::DataOperation> MakeTypicalStreamOperations();
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_TEST_STREAM_BUILDER_H_
