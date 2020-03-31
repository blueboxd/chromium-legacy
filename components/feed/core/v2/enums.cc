// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/enums.h"

#include <ostream>

namespace feed {

// Included for debug builds only for reduced binary size.

std::ostream& operator<<(std::ostream& out, LoadStreamStatus value) {
#ifndef NDEBUG
  switch (value) {
    case LoadStreamStatus::kNoStatus:
      return out << "kNoStatus";
    case LoadStreamStatus::kLoadedFromStore:
      return out << "kLoadedFromStore";
    case LoadStreamStatus::kLoadedFromNetwork:
      return out << "kLoadedFromNetwork";
    case LoadStreamStatus::kFailedWithStoreError:
      return out << "kFailedWithStoreError";
    case LoadStreamStatus::kNoStreamDataInStore:
      return out << "kNoStreamDataInStore";
    case LoadStreamStatus::kModelAlreadyLoaded:
      return out << "kModelAlreadyLoaded";
    case LoadStreamStatus::kNoResponseBody:
      return out << "kNoResponseBody";
    case LoadStreamStatus::kProtoTranslationFailed:
      return out << "kProtoTranslationFailed";
  }
#else
  return out << (static_cast<int>(value));
#endif  // ifndef NDEBUG
}

}  // namespace feed
