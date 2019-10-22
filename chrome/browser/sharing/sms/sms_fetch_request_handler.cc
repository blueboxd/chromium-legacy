// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sms/sms_fetch_request_handler.h"

#include "base/logging.h"
#include "chrome/browser/sharing/proto/sms_fetch_request.pb.h"

SmsFetchRequestHandler::SmsFetchRequestHandler() = default;

SmsFetchRequestHandler::~SmsFetchRequestHandler() = default;

void SmsFetchRequestHandler::OnMessage(
    const chrome_browser_sharing::SharingMessage& message) {
  DCHECK(message.has_sms_fetch_request());
  // TODO(crbug.com/1015645): implementation left pending deliberately.
  NOTIMPLEMENTED();
}
