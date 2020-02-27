// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/security_token_pin/error_generator.h"

#include "base/i18n/number_formatting.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace security_token_pin {

base::string16 GenerateErrorMessage(ErrorLabel error_label,
                                    int attempts_left,
                                    bool accept_input) {
  base::string16 error_message;
  switch (error_label) {
    case ErrorLabel::kInvalidPin:
      error_message =
          l10n_util::GetStringUTF16(IDS_REQUEST_PIN_DIALOG_INVALID_PIN_ERROR);
      break;
    case ErrorLabel::kInvalidPuk:
      error_message =
          l10n_util::GetStringUTF16(IDS_REQUEST_PIN_DIALOG_INVALID_PUK_ERROR);
      break;
    case ErrorLabel::kMaxAttemptsExceeded:
      error_message = l10n_util::GetStringUTF16(
          IDS_REQUEST_PIN_DIALOG_MAX_ATTEMPTS_EXCEEDED_ERROR);
      break;
    case ErrorLabel::kUnknown:
      error_message =
          l10n_util::GetStringUTF16(IDS_REQUEST_PIN_DIALOG_UNKNOWN_ERROR);
      break;
    case ErrorLabel::kNone:
      if (attempts_left < 0)
        return base::string16();
      break;
  }

  if (!accept_input) {
    return error_message;
  }
  if (attempts_left == -1) {
    return l10n_util::GetStringFUTF16(IDS_REQUEST_PIN_DIALOG_ERROR_RETRY,
                                      error_message);
  }
  if (error_message.empty()) {
    return l10n_util::GetStringFUTF16(IDS_REQUEST_PIN_DIALOG_ATTEMPTS_LEFT,
                                      base::FormatNumber(attempts_left));
  }
  return l10n_util::GetStringFUTF16(IDS_REQUEST_PIN_DIALOG_ERROR_RETRY_ATTEMPTS,
                                    error_message,
                                    base::FormatNumber(attempts_left));
}

}  // namespace security_token_pin
}  // namespace chromeos
