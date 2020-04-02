// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/tasks/load_stream_task.h"

#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/wire/client_info.pb.h"
#include "components/feed/core/proto/v2/wire/feed_request.pb.h"
#include "components/feed/core/proto/v2/wire/request.pb.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/stream_model.h"
#include "components/feed/core/v2/stream_model_update_request.h"

namespace feed {

LoadStreamTask::LoadStreamTask(FeedStream* stream,
                               base::OnceCallback<void(Result)> done_callback)
    : stream_(stream), done_callback_(std::move(done_callback)) {}

LoadStreamTask::~LoadStreamTask() = default;

void LoadStreamTask::Run() {
  // Phase 1.
  //  - Return early if the model is already loaded.
  //  - Try to load from persistent storage.

  // Don't load if the model is already loaded.
  if (stream_->GetModel()) {
    Done(LoadStreamStatus::kModelAlreadyLoaded);
    return;
  }

  load_from_store_task_ = std::make_unique<LoadStreamFromStoreTask>(
      stream_->GetStore(), stream_->GetClock(), stream_->GetUserClass(),
      base::BindOnce(&LoadStreamTask::LoadFromStoreComplete, GetWeakPtr()));
  load_from_store_task_->Execute(base::DoNothing());
}

void LoadStreamTask::LoadFromStoreComplete(
    LoadStreamFromStoreTask::Result result) {
  load_from_store_status_ = result.status;
  // Phase 2.
  //  - If loading from store works, update the model.
  //  - Otherwise, try to load from the network.

  if (result.status == LoadStreamStatus::kLoadedFromStore) {
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

  // TODO(harringtond): Add throttling.
  // TODO(harringtond): Request parameters here are all placeholder values.
  feedwire::Request request;
  feedwire::ClientInfo& client_info =
      *request.mutable_feed_request()->mutable_client_info();
  client_info.set_platform_type(feedwire::ClientInfo::ANDROID_ID);
  client_info.set_app_type(feedwire::ClientInfo::CHROME);
  request.mutable_feed_request()->mutable_feed_query()->set_reason(
      feedwire::FeedQuery::MANUAL_REFRESH);

  fetch_start_time_ = base::TimeTicks::Now();
  stream_->GetNetwork()->SendQueryRequest(
      request,
      base::BindOnce(&LoadStreamTask::QueryRequestComplete, GetWeakPtr()));
}

void LoadStreamTask::QueryRequestComplete(
    FeedNetwork::QueryRequestResult result) {
  DCHECK(!stream_->GetModel());
  if (!result.response_body) {
    Done(LoadStreamStatus::kNoResponseBody);
    return;
  }

  std::unique_ptr<StreamModelUpdateRequest> update_request =
      stream_->GetWireResponseTranslator()->TranslateWireResponse(
          *result.response_body, base::TimeTicks::Now() - fetch_start_time_,
          stream_->GetClock()->Now());
  if (!update_request) {
    Done(LoadStreamStatus::kProtoTranslationFailed);
    return;
  }

  stream_->GetStore()->SaveFullStream(
      std::make_unique<StreamModelUpdateRequest>(*update_request),
      base::DoNothing());

  auto model = std::make_unique<StreamModel>();
  model->Update(std::move(update_request));
  stream_->LoadModel(std::move(model));

  Done(LoadStreamStatus::kLoadedFromNetwork);
}

void LoadStreamTask::Done(LoadStreamStatus status) {
  Result result;
  result.load_from_store_status = load_from_store_status_;
  result.final_status = status;
  std::move(done_callback_).Run(result);
  TaskComplete();
}

}  // namespace feed
