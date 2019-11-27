// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.modelprovider;

/** Defines completion of a continuation token initiated request. */
public final class TokenCompleted {
    private final ModelCursor mModelCursor;

    public TokenCompleted(ModelCursor modelCursor) {
        this.mModelCursor = modelCursor;
    }

    /**
     * Returns a cursor representing the continuation from the point in the stream the continuation
     * token was found.
     */
    public ModelCursor getCursor() {
        return mModelCursor;
    }
}
