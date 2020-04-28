// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/win/hresult_status_helper.h"

#include "base/logging.h"

namespace media {

Status HresultToStatus(HRESULT hresult,
                       const char* message,
                       StatusCode code,
                       const base::Location& location) {
  if (SUCCEEDED(hresult))
    return OkStatus();

  return Status(code, message == nullptr ? "HRESULT" : message, location)
      .WithData("value", logging::SystemErrorCodeToString(hresult));
}

}  // namespace media
