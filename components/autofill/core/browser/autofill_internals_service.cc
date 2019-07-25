// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_internals_service.h"

namespace autofill {

LogBuffer& operator<<(LogBuffer& buf, LoggingScope scope) {
  if (!buf.active())
    return buf;
  return buf << Tag{"div"} << Attrib{"scope", LoggingScopeToString(scope)}
             << Attrib{"class", "log-entry"};
}

LogBuffer& operator<<(LogBuffer& buf, LogMessage message) {
  if (!buf.active())
    return buf;
  return buf << Tag{"div"} << Attrib{"message", LogMessageToString(message)}
             << Attrib{"class", "log-message"} << LogMessageValue(message);
}

}  // namespace autofill
