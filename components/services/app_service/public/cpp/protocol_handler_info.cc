// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/protocol_handler_info.h"

#include <ostream>

namespace apps {

ProtocolHandlerInfo::ProtocolHandlerInfo() = default;

ProtocolHandlerInfo::ProtocolHandlerInfo(const ProtocolHandlerInfo& other) =
    default;

ProtocolHandlerInfo::~ProtocolHandlerInfo() = default;

bool operator==(const ProtocolHandlerInfo& handler1,
                const ProtocolHandlerInfo& handler2) {
  return handler1.protocol == handler2.protocol && handler1.url == handler2.url;
}

std::ostream& operator<<(std::ostream& out,
                         const ProtocolHandlerInfo& handler) {
  return out << "protocol: " << handler.protocol << " url: " << handler.url;
}

}  // namespace apps
