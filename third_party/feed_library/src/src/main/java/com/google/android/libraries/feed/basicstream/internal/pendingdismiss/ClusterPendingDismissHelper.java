// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream.internal.pendingdismiss;

import com.google.android.libraries.feed.sharedstream.pendingdismiss.PendingDismissCallback;
import com.google.search.now.ui.action.FeedActionProto.UndoAction;

/**
 * Interface that is passed through the stream hierarchy so during a dismissLocal we can find the
 * cluster FeatureDriver and pass that to the StreamDriver to handle a pending dismissLocal.
 */
public interface ClusterPendingDismissHelper {
    /**
     * Triggers the temporary removal of the content with snackbar. Content will either come back or
     * be fully removed based on the interactions with the snackbar.
     *
     * @param undoAction - Information for the rendering of the snackbar.
     * @param pendingDismissCallback - Callbacks to call once content has been committed or
     *         reversed.
     */
    void triggerPendingDismissForCluster(
            UndoAction undoAction, PendingDismissCallback pendingDismissCallback);
}
