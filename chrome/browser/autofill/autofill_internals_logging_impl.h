// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_INTERNALS_LOGGING_IMPL_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_INTERNALS_LOGGING_IMPL_H_

#include "base/macros.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_internals_logging.h"
#include "content/public/browser/web_ui.h"

namespace autofill {
class AutofillInternalsLogging;

class AutofillInternalsLoggingImpl : public AutofillInternalsLogging {
 public:
  AutofillInternalsLoggingImpl();
  ~AutofillInternalsLoggingImpl() override;

  void set_web_ui(content::WebUI* web_ui);

 private:
  static content::WebUI* web_ui_;

  void LogHelper(const base::Value& message) override;
  void LogRawHelper(const base::Value& message) override;

  DISALLOW_COPY_AND_ASSIGN(AutofillInternalsLoggingImpl);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_INTERNALS_LOGGING_IMPL_H_
