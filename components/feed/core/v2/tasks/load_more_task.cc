// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/tasks/load_more_task.h"

#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/check.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/wire/client_info.pb.h"
#include "components/feed/core/proto/v2/wire/feed_request.pb.h"
#include "components/feed/core/proto/v2/wire/request.pb.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/feed/core/v2/protocol_translator.h"
#include "components/feed/core/v2/stream_model.h"

namespace feed {

LoadMoreTask::LoadMoreTask(FeedStream* stream,
                           base::OnceCallback<void(Result)> done_callback)
    : stream_(stream), done_callback_(std::move(done_callback)) {}

LoadMoreTask::~LoadMoreTask() = default;

void LoadMoreTask::Run() {
  // Check prerequisites.
  StreamModel* model = stream_->GetModel();
  if (!model)
    return Done(LoadStreamStatus::kLoadMoreModelIsNotLoaded);

  LoadStreamStatus final_status =
      stream_->ShouldMakeFeedQueryRequest(/*is_load_more=*/true);
  if (final_status != LoadStreamStatus::kNoStatus)
    return Done(final_status);

  // Send network request.
  fetch_start_time_ = stream_->GetTickClock()->NowTicks();
  stream_->GetNetwork()->SendQueryRequest(
      CreateFeedQueryLoadMoreRequest(stream_->GetRequestMetadata(),
                                     model->GetConsistencyToken(),
                                     model->GetNextPageToken()),
      base::BindOnce(&LoadMoreTask::QueryRequestComplete, GetWeakPtr()));
}

void LoadMoreTask::QueryRequestComplete(
    FeedNetwork::QueryRequestResult result) {
  StreamModel* model = stream_->GetModel();
  DCHECK(model) << "Model was unloaded outside of a Task";

  if (!result.response_body)
    return Done(LoadStreamStatus::kNoResponseBody);

  RefreshResponseData translated_response =
      stream_->GetWireResponseTranslator()->TranslateWireResponse(
          *result.response_body,
          StreamModelUpdateRequest::Source::kNetworkLoadMore,
          stream_->GetClock()->Now());

  if (!translated_response.model_update_request)
    return Done(LoadStreamStatus::kProtoTranslationFailed);

  model->Update(std::move(translated_response.model_update_request));

  if (translated_response.request_schedule)
    stream_->SetRequestSchedule(*translated_response.request_schedule);

  Done(LoadStreamStatus::kLoadedFromNetwork);
}

void LoadMoreTask::Done(LoadStreamStatus status) {
  std::move(done_callback_).Run(Result(status));
  TaskComplete();
}

}  // namespace feed
