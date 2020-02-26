// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/uma_helper.h"

#include "base/metrics/histogram_functions.h"

namespace dom_distiller {

// static
void UMAHelper::RecordReaderModeEntry(ReaderModeEntryPoint entry_point) {
  // Use histograms instead of user actions because order doesn't matter.
  base::UmaHistogramEnumeration("DomDistiller.ReaderMode.EntryPoint",
                                entry_point);
}

// static
void UMAHelper::RecordReaderModeExit(ReaderModeEntryPoint exit_point) {
  // Use histograms instead of user actions because order doesn't matter.
  base::UmaHistogramEnumeration("DomDistiller.ReaderMode.ExitPoint",
                                exit_point);
}

}  // namespace dom_distiller
