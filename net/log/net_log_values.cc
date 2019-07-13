// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/net_log_values.h"

#include "base/values.h"
#include "net/log/net_log.h"

namespace net {

base::Value NetLogParamsWithInt(base::StringPiece name, int value) {
  base::Value params(base::Value::Type::DICTIONARY);
  params.SetIntKey(name, value);
  return params;
}

base::Value NetLogParamsWithInt64(base::StringPiece name, int64_t value) {
  base::DictionaryValue event_params;
  event_params.SetKey(name, NetLogNumberValue(value));
  return std::move(event_params);
}

base::Value NetLogParamsWithBool(base::StringPiece name, bool value) {
  base::Value params(base::Value::Type::DICTIONARY);
  params.SetBoolKey(name, value);
  return params;
}

base::Value NetLogParamsWithString(base::StringPiece name,
                                   base::StringPiece value) {
  base::Value params(base::Value::Type::DICTIONARY);
  params.SetStringKey(name, value);
  return params;
}

}  // namespace net
