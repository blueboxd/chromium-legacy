// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_INDEX_CONTENT_INDEX_METRICS_H_
#define CHROME_BROWSER_CONTENT_INDEX_CONTENT_INDEX_METRICS_H_

#include "third_party/blink/public/mojom/content_index/content_index.mojom.h"

namespace content_index {

// Records the category of the Content Index entry after registration.
void RecordContentAdded(blink::mojom::ContentCategory category);

// Records the category of the Content Index entry when a user opens it.
void RecordContentOpened(blink::mojom::ContentCategory category);

// Records the number of Content Index entries available when requested.
void RecordContentIndexEntries(size_t num_entries);

}  // namespace content_index

#endif  // CHROME_BROWSER_CONTENT_INDEX_CONTENT_INDEX_METRICS_H_
