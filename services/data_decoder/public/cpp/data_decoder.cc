// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/data_decoder.h"

#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "services/data_decoder/public/mojom/json_parser.mojom.h"

#if defined(OS_ANDROID)
#include "base/json/json_reader.h"
#include "services/data_decoder/public/cpp/json_sanitizer.h"
#endif

namespace data_decoder {

namespace {

// The amount of idle time to tolerate on a DataDecoder instance. If the
// instance is unused for this period of time, the underlying service process
// (if any) may be killed and only restarted once needed again.
//
// On platforms (like iOS) or environments (like some unit tests) where
// out-of-process services are not used, this has no effect.
constexpr base::TimeDelta kServiceProcessIdleTimeout{
    base::TimeDelta::FromSeconds(5)};

#if !defined(OS_ANDROID)
// Encapsulates an in-process JSON parsing request. This provides shared
// ownership of the caller's callback so that it may be invoked exactly once by
// *either* the successful response handler *or* the JsonParser's disconnection
// handler. This also owns a Remote<mojom::JsonParser> which is kept alive for
// the duration of the request.
class ValueParseRequest : public base::RefCounted<ValueParseRequest> {
 public:
  explicit ValueParseRequest(DataDecoder::ValueParseCallback callback)
      : callback_(std::move(callback)) {}

  mojo::Remote<mojom::JsonParser>& json_parser() { return json_parser_; }
  DataDecoder::ValueParseCallback& callback() { return callback_; }

 private:
  friend class base::RefCounted<ValueParseRequest>;

  ~ValueParseRequest() = default;

  mojo::Remote<mojom::JsonParser> json_parser_;
  DataDecoder::ValueParseCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(ValueParseRequest);
};

// Handles a successful JSON parse from the service.
void OnServiceValueOrError(scoped_refptr<ValueParseRequest> request,
                           base::Optional<base::Value> value,
                           const base::Optional<std::string>& error) {
  DataDecoder::ValueOrError result;
  if (value)
    result.value = std::move(value);
  else
    result.error = error.value_or("unknown error");
  std::move(request->callback()).Run(std::move(result));
}

// Handles unexpected disconnection from the service during a JSON parse
// request. This typically means the service crashed.
void OnJsonParserDisconnected(scoped_refptr<ValueParseRequest> request) {
  std::move(request->callback())
      .Run(DataDecoder::ValueOrError::Error(
          "Data Decoder terminated unexpectedly"));
}
#endif  // !defined(OS_ANDROID)

}  // namespace

DataDecoder::ValueOrError::ValueOrError() = default;

DataDecoder::ValueOrError::ValueOrError(ValueOrError&&) = default;

DataDecoder::ValueOrError::~ValueOrError() = default;

// static
DataDecoder::ValueOrError DataDecoder::ValueOrError::Value(base::Value value) {
  ValueOrError result;
  result.value = std::move(value);
  return result;
}

DataDecoder::ValueOrError DataDecoder::ValueOrError::Error(
    const std::string& error) {
  ValueOrError result;
  result.error = error;
  return result;
}

DataDecoder::DataDecoder() = default;

DataDecoder::~DataDecoder() = default;

mojom::DataDecoderService* DataDecoder::GetService() {
  // Lazily start an instance of the service if possible and necessary.
  if (!service_) {
    auto* provider = ServiceProvider::Get();
    if (!provider) {
      LOG(FATAL) << "data_decoder::ServiceProvider::Set() must be called "
                 << "before any instances of DataDecoder can be used.";
      return nullptr;
    }

    provider->BindDataDecoderService(service_.BindNewPipeAndPassReceiver());
    service_.reset_on_disconnect();
    service_.reset_on_idle_timeout(kServiceProcessIdleTimeout);
  }

  return service_.get();
}

void DataDecoder::ParseJson(const std::string& json,
                            ValueParseCallback callback) {
#if defined(OS_ANDROID)
  // For Android, we use the in-process sanitizer and then parse with a simple
  // JSONReader.
  JsonSanitizer::Sanitize(
      json, base::BindOnce(
                [](ValueParseCallback callback, JsonSanitizer::Result result) {
                  if (!result.value) {
                    std::move(callback).Run(ValueOrError::Error(*result.error));
                    return;
                  }

                  base::JSONReader::ValueWithError value_with_error =
                      base::JSONReader::ReadAndReturnValueWithError(
                          *result.value, base::JSON_PARSE_RFC);
                  if (!value_with_error.value) {
                    std::move(callback).Run(
                        ValueOrError::Error(value_with_error.error_message));
                    return;
                  }

                  std::move(callback).Run(
                      ValueOrError::Value(std::move(*value_with_error.value)));
                },
                std::move(callback)));
#else
  auto request = base::MakeRefCounted<ValueParseRequest>(std::move(callback));
  GetService()->BindJsonParser(
      request->json_parser().BindNewPipeAndPassReceiver());
  request->json_parser().set_disconnect_handler(
      base::BindOnce(&OnJsonParserDisconnected, request));
  request->json_parser()->Parse(
      json, base::BindOnce(&OnServiceValueOrError, request));
#endif
}

// static
void DataDecoder::ParseJsonIsolated(const std::string& json,
                                    ValueParseCallback callback) {
  auto decoder = std::make_unique<DataDecoder>();
  auto* raw_decoder = decoder.get();

  // We bind the DataDecoder's ownership into the result callback to ensure that
  // it stays alive until the operation is complete.
  raw_decoder->ParseJson(
      json, base::BindOnce(
                [](std::unique_ptr<DataDecoder>, ValueParseCallback callback,
                   ValueOrError result) {
                  std::move(callback).Run(std::move(result));
                },
                std::move(decoder), std::move(callback)));
}

}  // namespace data_decoder
