// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.store;

import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamPayload;

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
