// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.api.internal.store;

import com.google.android.libraries.feed.api.host.storage.CommitResult;
import com.google.search.now.feed.client.StreamDataProto.StreamPayload;

/**
 * Mutation for adding and updating Content in the Feed Store. Remove will be handled by a task
 * which garbage collects the content that is not longer defined in any sessions.
 */
public interface ContentMutation {
    /** Add a new Mutation to the Store */
    ContentMutation add(String contentId, StreamPayload payload);

    /** Commit the mutations to the backing store */
    CommitResult commit();
}
