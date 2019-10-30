// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/safe_json_parser_android.h"

#include <utility>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "services/data_decoder/public/cpp/json_sanitizer.h"

namespace data_decoder {

SafeJsonParserAndroid::SafeJsonParserAndroid(const std::string& unsafe_json,
                                             SuccessCallback success_callback,
                                             ErrorCallback error_callback)
    : unsafe_json_(unsafe_json),
      success_callback_(std::move(success_callback)),
      error_callback_(std::move(error_callback)) {}

SafeJsonParserAndroid::~SafeJsonParserAndroid() {}

void SafeJsonParserAndroid::Start() {
  using StringCallback = base::OnceCallback<void(const std::string&)>;
  JsonSanitizer::Sanitize(
      unsafe_json_,
      base::BindOnce(
          [](StringCallback success_callback, StringCallback error_callback,
             JsonSanitizer::Result result) {
            if (result.value)
              std::move(success_callback).Run(*result.value);
            else
              std::move(error_callback).Run(*result.error);
          },
          base::BindOnce(&SafeJsonParserAndroid::OnSanitizationSuccess,
                         base::Unretained(this)),
          base::BindOnce(&SafeJsonParserAndroid::OnSanitizationError,
                         base::Unretained(this))));
}

void SafeJsonParserAndroid::OnSanitizationSuccess(
    const std::string& sanitized_json) {
  // Self-destruct at the end of this method.
  std::unique_ptr<SafeJsonParserAndroid> deleter(this);

  base::JSONReader::ValueWithError value_with_error =
      base::JSONReader::ReadAndReturnValueWithError(sanitized_json,
                                                    base::JSON_PARSE_RFC);

  if (!value_with_error.value) {
    std::move(error_callback_).Run(value_with_error.error_message);
    return;
  }

  std::move(success_callback_).Run(std::move(*value_with_error.value));
}

void SafeJsonParserAndroid::OnSanitizationError(const std::string& error) {
  std::move(error_callback_).Run(error);
  delete this;
}

}  // namespace data_decoder
