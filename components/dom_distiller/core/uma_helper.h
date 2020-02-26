// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_UMA_HELPER_H_
#define COMPONENTS_DOM_DISTILLER_CORE_UMA_HELPER_H_

#include "base/macros.h"

namespace dom_distiller {

// A utility class for logging UMA metrics.
class UMAHelper {
 public:
  // Must agree with ReaderModeEntryPoint in enums.xml.
  enum class ReaderModeEntryPoint {
    kOmniboxIcon = 0,
    kMenuOption = 1,
    kMaxValue = kMenuOption,
  };

  static void RecordReaderModeEntry(ReaderModeEntryPoint entry_point);
  static void RecordReaderModeExit(ReaderModeEntryPoint exit_point);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(UMAHelper);
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_UMA_HELPER_H_
