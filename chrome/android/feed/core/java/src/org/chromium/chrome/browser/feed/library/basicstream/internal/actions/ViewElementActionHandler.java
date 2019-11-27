// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.feed.library.basicstream.internal.actions;

/** Interface used for communicating view and hide events */
public interface ViewElementActionHandler {
    /**
     * Called when a hide action has been performed on a {@link
     * org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedActionMetadata.
     *   ElementType}
     * value.
     */
    void onElementHide(int elementType);

    /**
     * Called when a view action has been performed on a {@link
     * org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedActionMetadata.
     *   ElementType}
     * value.
     */
    void onElementView(int elementType);
}
