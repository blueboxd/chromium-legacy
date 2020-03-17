// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/test/stream_builder.h"

#include "base/strings/string_number_conversions.h"

namespace feed {

ContentId MakeContentId(ContentId::Type type,
                        std::string content_domain,
                        int id_number) {
  ContentId id;
  id.set_content_domain(std::move(content_domain));
  id.set_type(type);
  id.set_id(id_number);
  return id;
}

ContentId MakeClusterId(int id_number) {
  return MakeContentId(ContentId::CLUSTER, "content", id_number);
}

ContentId MakeContentContentId(int id_number) {
  return MakeContentId(ContentId::FEATURE, "stories", id_number);
}

ContentId MakeRootId(int id_number) {
  return MakeContentId(ContentId::TYPE_UNDEFINED, "root", id_number);
}

feedstore::StreamStructure MakeStream(int id_number) {
  feedstore::StreamStructure result;
  result.set_type(feedstore::StreamStructure::STREAM);
  result.set_operation(feedstore::StreamStructure::UPDATE_OR_APPEND);
  *result.mutable_content_id() = MakeRootId(id_number);
  return result;
}

feedstore::StreamStructure MakeCluster(int id_number, ContentId parent) {
  feedstore::StreamStructure result;
  result.set_type(feedstore::StreamStructure::CLUSTER);
  result.set_operation(feedstore::StreamStructure::UPDATE_OR_APPEND);
  *result.mutable_content_id() = MakeClusterId(id_number);
  *result.mutable_parent_id() = parent;
  return result;
}

feedstore::StreamStructure MakeContentNode(int id_number, ContentId parent) {
  feedstore::StreamStructure result;
  result.set_type(feedstore::StreamStructure::CONTENT);
  result.set_operation(feedstore::StreamStructure::UPDATE_OR_APPEND);
  *result.mutable_content_id() = MakeContentContentId(id_number);
  *result.mutable_parent_id() = parent;
  return result;
}

feedstore::StreamStructure MakeRemove(ContentId id) {
  feedstore::StreamStructure result;
  result.set_operation(feedstore::StreamStructure::REMOVE);
  *result.mutable_content_id() = id;
  return result;
}

feedstore::Content MakeContent(int id_number) {
  feedstore::Content result;
  *result.mutable_content_id() = MakeContentContentId(id_number);
  result.set_frame("f:" + base::NumberToString(id_number));
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

std::vector<feedstore::DataOperation> MakeTypicalStreamOperations() {
  return {
      MakeOperation(MakeStream()),
      MakeOperation(MakeCluster(0, MakeRootId())),
      MakeOperation(MakeContentNode(0, MakeClusterId(0))),
      MakeOperation(MakeContent(0)),
      MakeOperation(MakeCluster(1, MakeRootId())),
      MakeOperation(MakeContentNode(1, MakeClusterId(1))),
      MakeOperation(MakeContent(1)),
  };
}

}  // namespace feed
