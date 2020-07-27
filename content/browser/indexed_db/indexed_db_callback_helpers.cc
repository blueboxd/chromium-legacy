// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_callback_helpers.h"

namespace content {
namespace indexed_db_callback_helpers_internal {

template <>
mojo::PendingReceiver<blink::mojom::IDBDatabaseGetAllResultSink> AbortCallback(
    base::WeakPtr<IndexedDBTransaction> transaction) {
  if (transaction)
    transaction->IncrementNumErrorsSent();
  return mojo::NullReceiver();
}

}  // namespace indexed_db_callback_helpers_internal
}  // namespace content
