// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/feedback_registration.h"

#include <fuchsia/feedback/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>

#include "base/fuchsia/process_context.h"
#include "base/strings/string_piece.h"
#include "components/version_info/version_info.h"

namespace cr_fuchsia {

void RegisterCrashReportingFields(base::StringPiece component_url,
                                  base::StringPiece crash_product_name) {
  fuchsia::feedback::CrashReportingProduct product_data;
  product_data.set_name(crash_product_name.as_string());
  product_data.set_version(version_info::GetVersionNumber());
  // TODO(https://crbug.com/1077428): Use the actual channel when appropriate.
  // For now, always set it to the empty string to avoid reporting "missing".
  product_data.set_channel("");
  base::ComponentContextForProcess()
      ->svc()
      ->Connect<fuchsia::feedback::CrashReportingProductRegister>()
      ->Upsert(component_url.as_string(), std::move(product_data));
}

}  // namespace cr_fuchsia
