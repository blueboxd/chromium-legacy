// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/stream_model_update_request.h"

#include <utility>

#include "base/optional.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/wire/data_operation.pb.h"
#include "components/feed/core/proto/v2/wire/feature.pb.h"
#include "components/feed/core/proto/v2/wire/feed_response.pb.h"
#include "components/feed/core/proto/v2/wire/payload_metadata.pb.h"
#include "components/feed/core/proto/v2/wire/stream_structure.pb.h"
#include "components/feed/core/proto/v2/wire/token.pb.h"
#include "components/feed/core/v2/proto_util.h"

namespace feed {

namespace {

feedstore::StreamStructure::Operation TranslateOperationType(
    feedwire::DataOperation::Operation operation) {
  switch (operation) {
    case feedwire::DataOperation::UNKNOWN_OPERATION:
      return feedstore::StreamStructure::UNKNOWN;
    case feedwire::DataOperation::CLEAR_ALL:
      return feedstore::StreamStructure::CLEAR_ALL;
    case feedwire::DataOperation::UPDATE_OR_APPEND:
      return feedstore::StreamStructure::UPDATE_OR_APPEND;
    case feedwire::DataOperation::REMOVE:
      return feedstore::StreamStructure::REMOVE;
    default:
      return feedstore::StreamStructure::UNKNOWN;
  }
}

feedstore::StreamStructure::Type TranslateNodeType(
    feedwire::Feature::RenderableUnit renderable_unit) {
  switch (renderable_unit) {
    case feedwire::Feature::UNKNOWN_RENDERABLE_UNIT:
      return feedstore::StreamStructure::UNKNOWN_TYPE;
    case feedwire::Feature::STREAM:
      return feedstore::StreamStructure::STREAM;
    case feedwire::Feature::CONTENT:
      return feedstore::StreamStructure::CONTENT;
    case feedwire::Feature::CLUSTER:
      return feedstore::StreamStructure::CLUSTER;
    default:
      return feedstore::StreamStructure::UNKNOWN_TYPE;
  }
}

struct ConvertedDataOperation {
  bool has_stream_structure = false;
  feedstore::StreamStructure stream_structure;
  bool has_content = false;
  feedstore::Content content;
  bool has_shared_state = false;
  feedstore::StreamSharedState shared_state;
};

bool TranslateFeature(feedwire::Feature* feature,
                      ConvertedDataOperation* result) {
  feedstore::StreamStructure::Type type =
      TranslateNodeType(feature->renderable_unit());
  result->stream_structure.set_type(type);

  if (type == feedstore::StreamStructure::CONTENT) {
    feedwire::Content* wire_content = feature->mutable_content_extension();

    if (wire_content->type() != feedwire::Content::XSURFACE)
      return false;

    // TODO(iwells): We still need score, availability_time_seconds,
    // offline_metadata, and representation_data to populate content_info.

    *(result->content.mutable_content_id()) =
        result->stream_structure.content_id();
    result->content.set_allocated_frame(
        wire_content->mutable_xsurface_content()->release_xsurface_output());
    result->has_content = true;
  }
  return true;
}

base::Optional<feedstore::StreamSharedState> TranslateSharedState(
    feedwire::ContentId content_id,
    feedwire::RenderData* wire_shared_state) {
  if (wire_shared_state->render_data_type() != feedwire::RenderData::XSURFACE) {
    return base::nullopt;
  }

  feedstore::StreamSharedState shared_state;
  *shared_state.mutable_content_id() = std::move(content_id);
  shared_state.set_allocated_shared_state_data(
      wire_shared_state->mutable_xsurface_container()->release_render_data());
  return shared_state;
}

bool TranslatePayload(feedwire::DataOperation operation,
                      ConvertedDataOperation* result) {
  switch (operation.payload_case()) {
    case feedwire::DataOperation::kFeature: {
      feedwire::Feature* feature = operation.mutable_feature();
      result->stream_structure.set_allocated_parent_id(
          feature->release_parent_id());

      if (!TranslateFeature(feature, result))
        return false;
    } break;
    case feedwire::DataOperation::kNextPageToken: {
      feedwire::Token* token = operation.mutable_next_page_token();
      result->stream_structure.set_allocated_parent_id(
          token->release_parent_id());
      // TODO(iwells): We should be setting token bytes here.
      // result->stream_structure.set_allocated_next_page_token(
      //   token->MutableExtension(
      //     components::feed::core::proto::ui
      //       ::stream::NextPageToken::next_page_token_extension
      //   )->release_next_page_token());
    } break;
    case feedwire::DataOperation::kRenderData: {
      base::Optional<feedstore::StreamSharedState> shared_state =
          TranslateSharedState(result->stream_structure.content_id(),
                               operation.mutable_render_data());
      if (!shared_state)
        return false;

      result->shared_state = std::move(shared_state.value());
      result->has_shared_state = true;
    } break;
    // Fall through
    case feedwire::DataOperation::kInPlaceUpdateHandle:
    case feedwire::DataOperation::kTemplates:
    case feedwire::DataOperation::PAYLOAD_NOT_SET:
    default:
      return false;
  }

  return true;
}

base::Optional<ConvertedDataOperation> TranslateDataOperationInternal(
    feedwire::DataOperation operation) {
  feedstore::StreamStructure::Operation operation_type =
      TranslateOperationType(operation.operation());

  ConvertedDataOperation result;
  result.stream_structure.set_operation(operation_type);
  result.has_stream_structure = true;

  switch (operation_type) {
    case feedstore::StreamStructure::CLEAR_ALL:
      return result;

    case feedstore::StreamStructure::UPDATE_OR_APPEND:
      if (!operation.has_metadata() || !operation.metadata().has_content_id())
        return base::nullopt;

      result.stream_structure.set_allocated_content_id(
          operation.mutable_metadata()->release_content_id());

      if (!TranslatePayload(std::move(operation), &result))
        return base::nullopt;
      break;

    case feedstore::StreamStructure::REMOVE:
      if (!operation.has_metadata() || !operation.metadata().has_content_id())
        return base::nullopt;

      result.stream_structure.set_allocated_content_id(
          operation.mutable_metadata()->release_content_id());
      break;

    case feedstore::StreamStructure::UNKNOWN:  // Fall through
    default:
      return base::nullopt;
  }

  return result;
}

}  // namespace

StreamModelUpdateRequest::StreamModelUpdateRequest() = default;
StreamModelUpdateRequest::~StreamModelUpdateRequest() = default;
StreamModelUpdateRequest::StreamModelUpdateRequest(
    const StreamModelUpdateRequest&) = default;
StreamModelUpdateRequest& StreamModelUpdateRequest::operator=(
    const StreamModelUpdateRequest&) = default;

base::Optional<feedstore::DataOperation> TranslateDataOperation(
    feedwire::DataOperation wire_operation) {
  feedstore::DataOperation store_operation;
  base::Optional<ConvertedDataOperation> converted =
      TranslateDataOperationInternal(std::move(wire_operation));
  if (!converted)
    return base::nullopt;

  if (!converted->has_stream_structure && !converted->has_content)
    return base::nullopt;

  *store_operation.mutable_structure() = std::move(converted->stream_structure);
  *store_operation.mutable_content() = std::move(converted->content);
  return store_operation;
}

std::unique_ptr<StreamModelUpdateRequest> TranslateWireResponse(
    feedwire::Response response,
    base::TimeDelta response_time,
    base::Time current_time) {
  if (response.response_version() != feedwire::Response::FEED_RESPONSE)
    return nullptr;

  auto result = std::make_unique<StreamModelUpdateRequest>();

  feedwire::FeedResponse* feed_response = response.mutable_feed_response();
  for (auto& wire_data_operation : *feed_response->mutable_data_operation()) {
    if (!wire_data_operation.has_operation())
      continue;

    base::Optional<ConvertedDataOperation> operation =
        TranslateDataOperationInternal(std::move(wire_data_operation));
    if (!operation)
      continue;

    if (operation->has_stream_structure) {
      result->stream_structures.push_back(
          std::move(operation->stream_structure));
    }

    if (operation->has_content)
      result->content.push_back(std::move(operation.value().content));

    if (operation->has_shared_state)
      result->shared_states.push_back(std::move(operation->shared_state));
  }

  // TODO(harringtond): If there's more than one shared state, record some
  // sort of error.
  if (!result->shared_states.empty()) {
    *result->stream_data.mutable_shared_state_id() =
        result->shared_states.front().content_id();
  }
  feedstore::SetLastAddedTime(current_time, &result->stream_data);
  result->server_response_time =
      feed_response->feed_response_metadata().response_time_ms();
  result->response_time = response_time;

  return result;
}

}  // namespace feed
