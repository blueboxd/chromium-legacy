// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "base/util/values/values_util.h"

namespace network {
namespace internal {

base::Optional<base::Time> StringToTime(base::StringPiece my_string) {
  return util::ValueToTime(base::Value(my_string));
}

std::string TimeToString(base::Time my_time) {
  return util::TimeToValue(my_time).GetString();
}

}  // namespace internal
}  // namespace network
