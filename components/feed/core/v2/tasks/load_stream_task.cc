// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/tasks/load_stream_task.h"

#include <memory>
#include <utility>

#include "base/logging.h"
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
                               base::OnceClosure done_callback)
    : stream_(stream), done_callback_(std::move(done_callback)) {}

LoadStreamTask::~LoadStreamTask() = default;

void LoadStreamTask::Run() {
  // TODO(harringtond): This logic is all provisional and should be rewritten.
  // Don't load if the model is already loaded.
  if (stream_->GetModel()) {
    Done();
    return;
  }

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
    Done();
    return;
  }

  std::unique_ptr<StreamModelUpdateRequest> update_request =
      stream_->GetWireResponseTranslator()->TranslateWireResponse(
          *result.response_body, base::TimeTicks::Now() - fetch_start_time_);
  if (!update_request) {
    Done();
    return;
  }

  auto model = std::make_unique<StreamModel>();
  model->Update(std::move(update_request));
  stream_->LoadModel(std::move(model));

  Done();
}

void LoadStreamTask::Done() {
  std::move(done_callback_).Run();
  TaskComplete();
}

}  // namespace feed
