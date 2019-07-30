// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_index/content_index_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace content_index {

void RecordContentAdded(blink::mojom::ContentCategory category) {
  base::UmaHistogramEnumeration("ContentIndex.ContentAdded", category);
}

void RecordContentOpened(blink::mojom::ContentCategory category) {
  base::UmaHistogramEnumeration("ContentIndex.ContentOpened", category);
}

void RecordContentIndexEntries(size_t num_entries) {
  base::UmaHistogramCounts1000("ContentIndex.NumEntriesAvailable", num_entries);
}

}  // namespace content_index
