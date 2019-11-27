// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.knowncontent;

import org.chromium.chrome.browser.feed.library.api.client.knowncontent.KnownContent;

import java.util.List;

/** Allows the feed libraries to request and subscribe to information about the Feed's content. */
public interface FeedKnownContent extends KnownContent {
    /**
     * Gets listener that notifies all added listeners of {@link
     * KnownContent.Listener#onContentRemoved(List)} or {@link
     * KnownContent.Listener#onNewContentReceived(boolean, long)}.
     *
     * <p>Note: This method is internal to the Feed. It provides a {@link Listener} that, when
     * notified, will propagate the notification to the host.
     */
    KnownContent.Listener getKnownContentHostNotifier();
}
