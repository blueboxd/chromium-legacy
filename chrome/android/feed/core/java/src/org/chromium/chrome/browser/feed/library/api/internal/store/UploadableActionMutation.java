// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.store;

import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamUploadableAction;

/** Mutation for adding and updating uploadable actions in the Feed Store. */
public interface UploadableActionMutation {
    /** Upsert a new Mutation to the Store */
    UploadableActionMutation upsert(StreamUploadableAction action, String contentId);

    /** Remove Mutation from the Store */
    UploadableActionMutation remove(StreamUploadableAction action, String contentId);

    /** Commit the mutations to the backing store */
    CommitResult commit();
}
