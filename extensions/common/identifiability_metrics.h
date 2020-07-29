// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_IDENTIFIABILITY_METRICS_H_
#define EXTENSIONS_COMMON_IDENTIFIABILITY_METRICS_H_

#include "base/metrics/ukm_source_id.h"

class GURL;

namespace extensions {

// Used for histograms. Do not reorder.
enum class ExtensionResourceAccessResult : int {
  kSuccess,
  kCancel,   // Only logged on navigation when the navigation is cancelled and
             // the document stays in place.
  kFailure,  // resource load failed or navigation to some sort of error page.
};

// Records results of attempts to access an extension resource at the url
// |gurl|. Done as part of a study to see if this is being used as a
// fingerprinting method.
void RecordExtensionResourceAccessResult(base::UkmSourceId ukm_source_id,
                                         const GURL& gurl,
                                         ExtensionResourceAccessResult result);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_IDENTIFIABILITY_METRICS_H_
