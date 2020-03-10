// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feed_network.h"

#include "components/feed/core/proto/v2/wire/action_request.pb.h"
#include "components/feed/core/proto/v2/wire/feed_action_response.pb.h"
#include "components/feed/core/proto/v2/wire/request.pb.h"
#include "components/feed/core/proto/v2/wire/response.pb.h"

namespace feed {

FeedNetwork::QueryRequestResult::QueryRequestResult() = default;
FeedNetwork::QueryRequestResult::~QueryRequestResult() = default;
FeedNetwork::QueryRequestResult::QueryRequestResult(QueryRequestResult&&) =
    default;
FeedNetwork::QueryRequestResult& FeedNetwork::QueryRequestResult::operator=(
    QueryRequestResult&&) = default;

FeedNetwork::ActionRequestResult::ActionRequestResult() = default;
FeedNetwork::ActionRequestResult::~ActionRequestResult() = default;
FeedNetwork::ActionRequestResult::ActionRequestResult(ActionRequestResult&&) =
    default;
FeedNetwork::ActionRequestResult& FeedNetwork::ActionRequestResult::operator=(
    ActionRequestResult&&) = default;

FeedNetwork::~FeedNetwork() = default;

}  // namespace feed
