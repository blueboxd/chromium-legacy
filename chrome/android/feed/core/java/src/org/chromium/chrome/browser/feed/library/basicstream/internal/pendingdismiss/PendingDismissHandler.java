// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.pendingdismiss;

import org.chromium.chrome.browser.feed.library.sharedstream.pendingdismiss.PendingDismissCallback;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.UndoAction;

/** Interface that handles dismissing a card with an undo option. */
public interface PendingDismissHandler {
    /**
     * Triggers the temporary removal of the content with snackbar. Content will either come back or
     * be fully removed based on the interactions with the snackbar.
     *
     * @param contentId - The content that should be temporary hidden until the dismiss is
     *         committed.
     * @param undoAction - Information for the rendering of the snackbar.
     * @param pendingDismissCallback - Callbacks to call once content has been committed or
     *         reversed.
     */
    void triggerPendingDismiss(
            String contentId, UndoAction undoAction, PendingDismissCallback pendingDismissCallback);
}
