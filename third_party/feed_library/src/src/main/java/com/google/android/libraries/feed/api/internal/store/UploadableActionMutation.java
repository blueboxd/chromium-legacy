// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.api.internal.store;

import com.google.android.libraries.feed.api.host.storage.CommitResult;
import com.google.search.now.feed.client.StreamDataProto.StreamUploadableAction;

/** Mutation for adding and updating uploadable actions in the Feed Store. */
public interface UploadableActionMutation {
    /** Upsert a new Mutation to the Store */
    UploadableActionMutation upsert(StreamUploadableAction action, String contentId);

    /** Remove Mutation from the Store */
    UploadableActionMutation remove(StreamUploadableAction action, String contentId);

    /** Commit the mutations to the backing store */
    CommitResult commit();
}
