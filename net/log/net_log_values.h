// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_LOG_NET_LOG_VALUES_H_
#define NET_LOG_NET_LOG_VALUES_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/strings/string_piece_forward.h"
#include "net/base/net_export.h"

namespace base {
class Value;
}

namespace net {

// Helpers to construct simple dictionaries with a single key/value.
NET_EXPORT base::Value NetLogParamsWithInt(base::StringPiece name, int value);
NET_EXPORT base::Value NetLogParamsWithInt64(base::StringPiece name,
                                             int64_t value);
NET_EXPORT base::Value NetLogParamsWithBool(base::StringPiece name, bool value);
NET_EXPORT base::Value NetLogParamsWithString(base::StringPiece name,
                                              base::StringPiece value);

}  // namespace net

#endif  // NET_LOG_NET_LOG_VALUES_H_
