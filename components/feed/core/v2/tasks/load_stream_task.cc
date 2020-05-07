// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/tasks/load_stream_task.h"

#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/check.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/wire/capability.pb.h"
#include "components/feed/core/proto/v2/wire/client_info.pb.h"
#include "components/feed/core/proto/v2/wire/feed_request.pb.h"
#include "components/feed/core/proto/v2/wire/request.pb.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/feed/core/v2/protocol_translator.h"
#include "components/feed/core/v2/stream_model.h"

namespace feed {
namespace {
using LoadType = LoadStreamTask::LoadType;
using Result = LoadStreamTask::Result;

feedwire::FeedQuery::RequestReason GetRequestReason(LoadType load_type) {
  switch (load_type) {
    case LoadType::kInitialLoad:
      return feedwire::FeedQuery::MANUAL_REFRESH;
    case LoadType::kBackgroundRefresh:
      return feedwire::FeedQuery::SCHEDULED_REFRESH;
  }
}

}  // namespace

Result::Result() = default;
Result::Result(LoadStreamStatus status) : final_status(status) {}
Result::~Result() = default;
Result::Result(const Result&) = default;
Result& Result::operator=(const Result&) = default;

LoadStreamTask::LoadStreamTask(LoadType load_type,
                               FeedStream* stream,
                               base::OnceCallback<void(Result)> done_callback)
    : load_type_(load_type),
      stream_(stream),
      done_callback_(std::move(done_callback)) {}

LoadStreamTask::~LoadStreamTask() = default;

void LoadStreamTask::Run() {
  // Phase 1: Try to load from persistent storage.

  // TODO(harringtond): We're checking ShouldAttemptLoad() here and before the
  // task is added to the task queue. Maybe we can simplify this.

  // First, ensure we still should load the model.
  LoadStreamStatus should_not_attempt_reason = stream_->ShouldAttemptLoad(
      /*model_loading=*/true);
  if (should_not_attempt_reason != LoadStreamStatus::kNoStatus) {
    return Done(should_not_attempt_reason);
  }

  // Use |kConsistencyTokenOnly| to short-circuit loading from store if we don't
  // need the full stream state.
  auto load_from_store_type =
      (load_type_ == LoadType::kInitialLoad)
          ? LoadStreamFromStoreTask::LoadType::kFullLoad
          : LoadStreamFromStoreTask::LoadType::kConsistencyTokenOnly;

  load_from_store_task_ = std::make_unique<LoadStreamFromStoreTask>(
      load_from_store_type, stream_->GetStore(), stream_->GetClock(),
      base::BindOnce(&LoadStreamTask::LoadFromStoreComplete, GetWeakPtr()));
  load_from_store_task_->Execute(base::DoNothing());
}

void LoadStreamTask::LoadFromStoreComplete(
    LoadStreamFromStoreTask::Result result) {
  load_from_store_status_ = result.status;
  // Phase 2.
  //  - If loading from store works, update the model.
  //  - Otherwise, try to load from the network.

  if (load_type_ == LoadType::kInitialLoad &&
      result.status == LoadStreamStatus::kLoadedFromStore) {
    auto model = std::make_unique<StreamModel>();
    model->Update(std::move(result.update_request));
    stream_->LoadModel(std::move(model));
    Done(LoadStreamStatus::kLoadedFromStore);
    return;
  }

  LoadStreamStatus final_status = stream_->ShouldMakeFeedQueryRequest();
  if (final_status != LoadStreamStatus::kNoStatus) {
    Done(final_status);
    return;
  }

  stream_->GetNetwork()->SendQueryRequest(
      CreateFeedQueryRefreshRequest(GetRequestReason(load_type_),
                                    stream_->GetRequestMetadata(),
                                    result.consistency_token),
      base::BindOnce(&LoadStreamTask::QueryRequestComplete, GetWeakPtr()));
}

void LoadStreamTask::QueryRequestComplete(
    FeedNetwork::QueryRequestResult result) {
  DCHECK(!stream_->GetModel());

  network_response_info_ = result.response_info;

  if (!result.response_body) {
    Done(LoadStreamStatus::kNoResponseBody);
    return;
  }

  RefreshResponseData response_data =
      stream_->GetWireResponseTranslator()->TranslateWireResponse(
          *result.response_body,
          StreamModelUpdateRequest::Source::kNetworkUpdate,
          stream_->GetClock()->Now());
  if (!response_data.model_update_request) {
    Done(LoadStreamStatus::kProtoTranslationFailed);
    return;
  }

  stream_->GetStore()->OverwriteStream(
      std::make_unique<StreamModelUpdateRequest>(
          *response_data.model_update_request),
      base::DoNothing());

  if (load_type_ != LoadType::kBackgroundRefresh) {
    auto model = std::make_unique<StreamModel>();
    model->Update(std::move(response_data.model_update_request));
    stream_->LoadModel(std::move(model));
  }

  if (response_data.request_schedule)
    stream_->SetRequestSchedule(*response_data.request_schedule);

  Done(LoadStreamStatus::kLoadedFromNetwork);
}

void LoadStreamTask::Done(LoadStreamStatus status) {
  Result result;
  result.load_from_store_status = load_from_store_status_;
  result.final_status = status;
  result.load_type = load_type_;
  result.network_response_info = network_response_info_;
  std::move(done_callback_).Run(result);
  TaskComplete();
}

}  // namespace feed
