// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.api.internal.store;

import com.google.android.libraries.feed.api.host.storage.CommitResult;
import com.google.protobuf.ByteString;

/** Used to commit mutations to semantic properties data within the {@link Store} */
public interface SemanticPropertiesMutation {
    /** Add a new semantic properties mutation */
    SemanticPropertiesMutation add(String contentId, ByteString semanticData);

    /** Commit the current mutations */
    CommitResult commit();
}
