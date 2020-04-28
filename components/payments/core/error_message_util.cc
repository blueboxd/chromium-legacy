// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/error_message_util.h"

#include <algorithm>
#include <vector>

#include "base/check.h"
#include "base/strings/string_util.h"
#include "components/payments/core/native_error_strings.h"

namespace payments {

std::string GetNotSupportedErrorMessage(const std::set<std::string>& methods) {
  if (methods.empty())
    return errors::kGenericPaymentMethodNotSupportedMessage;

  std::vector<std::string> with_quotes(methods.size());
  std::transform(
      methods.begin(), methods.end(), with_quotes.begin(),
      [](const std::string& method_name) { return "\"" + method_name + "\""; });

  std::string output;
  bool replaced = base::ReplaceChars(
      with_quotes.size() == 1
          ? errors::kSinglePaymentMethodNotSupportedFormat
          : errors::kMultiplePaymentMethodsNotSupportedFormat,
      "$", base::JoinString(with_quotes, ", "), &output);
  DCHECK(replaced);
  return output;
}

}  // namespace payments
