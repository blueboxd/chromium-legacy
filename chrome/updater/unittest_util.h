// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UNITTEST_UTIL_H_
#define CHROME_UPDATER_UNITTEST_UTIL_H_

#include <ostream>
#include <string>

#include "base/optional.h"
#include "chrome/updater/tag.h"
#include "chrome/updater/update_service.h"

// Externally-defined printers for base types.
namespace base {

template <class T>
std::ostream& operator<<(std::ostream& os, const base::Optional<T>& opt) {
  if (opt.has_value()) {
    return os << opt.value();
  } else {
    return os << "base::nullopt";
  }
}

}  // namespace base

// Externally-defined printers for chrome/updater-related types.
namespace updater {

extern const char kChromeAppId[];

namespace tagging {
std::ostream& operator<<(std::ostream&, const ErrorCode&);
std::ostream& operator<<(std::ostream&, const AppArgs::NeedsAdmin&);
std::ostream& operator<<(std::ostream&, const TagArgs::BrowserType&);
}  // namespace tagging

bool operator==(const UpdateService::UpdateState& lhs,
                const UpdateService::UpdateState& rhs);
inline bool operator!=(const UpdateService::UpdateState& lhs,
                       const UpdateService::UpdateState& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os,
                         const UpdateService::UpdateState& update_state);

}  // namespace updater

#endif  // CHROME_UPDATER_UNITTEST_UTIL_H_
