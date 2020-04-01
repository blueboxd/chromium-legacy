// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

/**
 * Interface to provide chromium calling points for a feed.
 */
public interface FeedActionsHandler {
    String KEY = "FeedActions";

    /**
     * Sends data back to the server when content is clicked.
     */
    void processThereAndBackAgainData(byte[] data);
}
