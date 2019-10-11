// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/article_entry.h"

#include "base/logging.h"

namespace dom_distiller {

bool IsEntryPageValid(const ArticleEntryPage& page) {
  return page.has_url();
}

bool IsEntryValid(const ArticleEntry& entry) {
  if (!entry.has_entry_id())
    return false;
  for (int i = 0; i < entry.pages_size(); ++i) {
    if (!IsEntryPageValid(entry.pages(i)))
      return false;
  }
  return true;
}

bool AreEntriesEqual(const ArticleEntry& left, const ArticleEntry& right) {
  DCHECK(IsEntryValid(left));
  DCHECK(IsEntryValid(right));
  return left.SerializeAsString() == right.SerializeAsString();
}

}  // namespace dom_distiller
