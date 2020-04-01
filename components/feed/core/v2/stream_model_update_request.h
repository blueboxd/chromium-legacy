// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_STREAM_MODEL_UPDATE_REQUEST_H_
#define COMPONENTS_FEED_CORE_V2_STREAM_MODEL_UPDATE_REQUEST_H_

#include <memory>
#include <vector>

#include "base/optional.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/data_operation.pb.h"
#include "components/feed/core/proto/v2/wire/response.pb.h"

namespace feed {

// Data for updating StreamModel. This can be sourced from the network or
// persistent storage.
struct StreamModelUpdateRequest {
 public:
  enum class Source {
    kNetworkUpdate,
    kInitialLoadFromStore,
  };

  StreamModelUpdateRequest();
  ~StreamModelUpdateRequest();
  StreamModelUpdateRequest(const StreamModelUpdateRequest&);
  StreamModelUpdateRequest& operator=(const StreamModelUpdateRequest&);

  // Whether this data originates is from the initial load of content from
  // the local data store.
  Source source = Source::kNetworkUpdate;

  // The set of Contents marked UPDATE_OR_APPEND in the response, in the order
  // in which they were received.
  std::vector<feedstore::Content> content;

  // Contains the root ContentId, tokens, a timestamp for when the most recent
  // content was added, and a list of ContentIds for clusters in the response.
  feedstore::StreamData stream_data;

  // The set of StreamSharedStates marked UPDATE_OR_APPEND in the order in which
  // they were received.
  std::vector<feedstore::StreamSharedState> shared_states;

  std::vector<feedstore::StreamStructure> stream_structures;

  // If this data originates from the network, this is the server-reported time
  // at which the request was fulfilled.
  // TODO(harringtond): Use this or remove it.
  int64_t server_response_time = 0;

  // If this data originates from the network, this is the time taken by the
  // server to produce the response.
  // TODO(harringtond): Use this or remove it.
  base::TimeDelta response_time;

  int32_t max_structure_sequence_number = 0;
};

base::Optional<feedstore::DataOperation> TranslateDataOperation(
    feedwire::DataOperation wire_operation);

std::unique_ptr<StreamModelUpdateRequest> TranslateWireResponse(
    feedwire::Response response,
    base::TimeDelta response_time,
    base::Time current_time);

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_STREAM_MODEL_UPDATE_REQUEST_H_
