// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/safe_json_parser_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "services/data_decoder/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace data_decoder {

SafeJsonParserImpl::SafeJsonParserImpl(
    service_manager::Connector* connector,
    const std::string& unsafe_json,
    SuccessCallback success_callback,
    ErrorCallback error_callback,
    const base::Optional<base::Token>& batch_id)
    : unsafe_json_(unsafe_json),
      success_callback_(std::move(success_callback)),
      error_callback_(std::move(error_callback)) {
  // If no batch ID has been provided, use a random instance ID to guarantee the
  // connection is to a new service running in its own process.
  connector->Connect(
      service_manager::ServiceFilter::ByNameWithId(
          mojom::kServiceName, batch_id.value_or(base::Token::CreateRandom())),
      json_parser_.BindNewPipeAndPassReceiver());
}

SafeJsonParserImpl::~SafeJsonParserImpl() = default;

void SafeJsonParserImpl::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  json_parser_.set_disconnect_handler(base::Bind(
      &SafeJsonParserImpl::OnConnectionError, base::Unretained(this)));
  json_parser_->Parse(
      std::move(unsafe_json_),
      base::Bind(&SafeJsonParserImpl::OnParseDone, base::Unretained(this)));
}

void SafeJsonParserImpl::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Shut down the utility process.
  json_parser_.reset();

  ReportResults(base::nullopt,
                "Connection error with the json parser process.");
}

void SafeJsonParserImpl::OnParseDone(base::Optional<base::Value> result,
                                     const base::Optional<std::string>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Shut down the utility process.
  json_parser_.reset();

  ReportResults(std::move(result), error.value_or(""));
}

void SafeJsonParserImpl::ReportResults(base::Optional<base::Value> parsed_json,
                                       const std::string& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error.empty() && parsed_json) {
    if (!success_callback_.is_null())
      std::move(success_callback_).Run(std::move(*parsed_json));
  } else {
    if (!error_callback_.is_null())
      std::move(error_callback_).Run(error);
  }

  // The parsing is done whether an error occured or not, so this instance can
  // be cleaned up.
  delete this;
}

}  // namespace data_decoder
