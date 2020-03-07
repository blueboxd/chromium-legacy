// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/stream_model_update_request.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/feed/core/proto/libraries/api/internal/stream_data.pb.h"
#include "components/feed/core/proto/ui/action/ui_feed_action.pb.h"
#include "components/feed/core/proto/ui/stream/stream_structure.pb.h"
#include "components/feed/core/proto/wire/data_operation.pb.h"
#include "components/feed/core/proto/wire/feature.pb.h"
#include "components/feed/core/proto/wire/feed_response.pb.h"
#include "components/feed/core/proto/wire/payload_metadata.pb.h"
#include "components/feed/core/proto/wire/piet_shared_state_item.pb.h"
#include "components/feed/core/proto/wire/token.pb.h"

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
  // TODO update when new wire protos are ready
  switch (renderable_unit) {
    case feedwire::Feature::UNKNOWN_RENDERABLE_UNIT:
      return feedstore::StreamStructure::UNKNOWN_TYPE;
    case feedwire::Feature::STREAM:
      return feedstore::StreamStructure::STREAM;
    case feedwire::Feature::CARD:
      return feedstore::StreamStructure::CARD;
    case feedwire::Feature::CONTENT:
      return feedstore::StreamStructure::CONTENT;
    case feedwire::Feature::CLUSTER:
      return feedstore::StreamStructure::CLUSTER;
    case feedwire::Feature::TOKEN:
      return feedstore::StreamStructure::UNKNOWN_TYPE;
    case feedwire::Feature::CAROUSEL:
      return feedstore::StreamStructure::UNKNOWN_TYPE;
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
    if (!feature->HasExtension(components::feed::core::proto::ui::stream::
                                   Content::content_extension)) {
      return false;
    }

    components::feed::core::proto::ui::stream::Content* wire_content =
        feature->MutableExtension(components::feed::core::proto::ui::stream::
                                      Content::content_extension);

    // TODO(iwells): Change PIET to XSURFACE
    if (wire_content->type() !=
        components::feed::core::proto::ui::stream::Content::PIET)
      return false;

    feedstore::ContentInfo* content_info =
        result->stream_structure.mutable_content_info();

    // TODO(iwells): BasicLoggingMetadata is deprecated. Find out if score and
    // availability time are needed still.
    content_info->set_score(wire_content->basic_logging_metadata().score());
    content_info->set_availability_time_seconds(
        wire_content->basic_logging_metadata()
            .GetExtension(
                components::feed::core::proto::libraries::api::internal::
                    ClientBasicLoggingMetadata::client_basic_logging_metadata)
            .availability_time_seconds());

    // TODO(iwells): Get representation data and offline metadata from new
    // protos
    content_info->mutable_representation_data()->set_allocated_uri(
        wire_content->mutable_representation_data()->release_uri());
    content_info->mutable_representation_data()->set_published_time_seconds(
        wire_content->representation_data().published_time_seconds());

    content_info->mutable_offline_metadata()->set_allocated_title(
        wire_content->mutable_offline_metadata()->release_title());
    content_info->mutable_offline_metadata()->set_allocated_image_url(
        wire_content->mutable_offline_metadata()->release_image_url());
    content_info->mutable_offline_metadata()->set_allocated_publisher(
        wire_content->mutable_offline_metadata()->release_publisher());
    content_info->mutable_offline_metadata()->set_allocated_favicon_url(
        wire_content->mutable_offline_metadata()->release_favicon_url());
    content_info->mutable_offline_metadata()->set_allocated_snippet(
        wire_content->mutable_offline_metadata()->release_snippet());

    *(result->content.mutable_content_id()) =
        result->stream_structure.content_id();
    // TODO(iwells): Set xsurface content here
    // result->content
    // .set_allocated_frame(wire_content->release_xsurface_frame());
    result->has_content = true;
  }
  return true;
}

/* TODO(iwells): Uncomment when new protocol is ready (and have this return
 * base::Optional)
feedstore::StreamSharedState
TranslateSharedState(feedwire::ContentId* content_id, feedwire::SharedState*
wire_shared_state) { if (wire_shared_state->shared_state_type() !=
feedwire::SharedState::XSURFACE) return nullptr;

  auto shared_state = std::make_unique<feedstore::StreamSharedState>();
  shared_state->set_allocated_content_id(content_id);
  shared_state->set_allocated_shared_state_data(
    wire_shared_state->release_shared_state_data());
  return shared_state;
}
*/

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
    case feedwire::DataOperation::kPietSharedState:
      // TODO(iwells): Replace this case when new protocol is ready
      result->has_shared_state = true;
      *result->shared_state.mutable_content_id() =
          result->stream_structure.content_id();
      break;
    /* TODO(iwells): Uncomment and clean up when new protocol is ready
    case feedwire::DataOperation::kNextPageToken:
      feedwire::Token* token = operation.mutable_next_page_token();
      result->stream_structure.set_allocated_parent_id(
        token->release_parent_id());
      result->stream_structure.set_allocated_next_page_token(
        token->MutableExtension(
          components::feed::core::proto::ui
            ::stream::NextPageToken::next_page_token_extension
        )->release_next_page_token());
      break;
    case feedwire::DataOperation::kSharedState:
      result->shared_state = std::move(TranslateSharedState(content_id,
    operation.mutable_render_data()));
      result->has_shared_state = true;
      break;
    */
    case feedwire::DataOperation::PAYLOAD_NOT_SET:
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

    case feedstore::StreamStructure::UNKNOWN:  // fall through
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

  if (!converted.value().has_stream_structure && !converted.value().has_content)
    return base::nullopt;

  *store_operation.mutable_structure() =
      std::move(converted.value().stream_structure);
  *store_operation.mutable_content() = std::move(converted.value().content);
  return store_operation;
}

std::unique_ptr<StreamModelUpdateRequest> TranslateWireResponse(
    feedwire::Response response,
    base::TimeDelta response_time) {
  if (response.response_version() != feedwire::Response::FEED_RESPONSE)
    return nullptr;

  if (!response.HasExtension(feedwire::FeedResponse::feed_response))
    return nullptr;

  auto result = std::make_unique<StreamModelUpdateRequest>();

  feedwire::FeedResponse* feed_response =
      response.MutableExtension(feedwire::FeedResponse::feed_response);
  for (auto& wire_data_operation : *feed_response->mutable_data_operation()) {
    if (!wire_data_operation.has_operation())
      continue;

    base::Optional<ConvertedDataOperation> operation =
        TranslateDataOperationInternal(std::move(wire_data_operation));
    if (!operation)
      continue;

    if (operation->has_stream_structure) {
      *result->stream_data.add_structures() =
          std::move(operation->stream_structure);
    }

    if (operation->has_content)
      result->content.push_back(std::move(operation.value().content));

    if (operation->has_shared_state)
      result->shared_states.push_back(std::move(operation->shared_state));
  }

  result->server_response_time =
      feed_response->feed_response_metadata().response_time_ms();
  result->response_time = response_time;
  return result;
}

}  // namespace feed
