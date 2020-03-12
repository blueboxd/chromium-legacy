// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.store;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;

/** Mutation for adding and updating actions in the Feed Store. */
public interface LocalActionMutation {
    @IntDef({ActionType.DISMISS})
    @interface ActionType {
        int DISMISS = 1;
    }

    /** Add a new Mutation to the Store */
    LocalActionMutation add(@ActionType int action, String contentId);

    /** Commit the mutations to the backing store */
    CommitResult commit();
}
