// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.client.knowncontent;

/** Information on the removal of a piece of content. */
public final class ContentRemoval {
    private final String mUrl;
    private final boolean mRequestedByUser;

    public ContentRemoval(String url, boolean requestedByUser) {
        this.mUrl = url;
        this.mRequestedByUser = requestedByUser;
    }

    /** Url for removed content. */
    public String getUrl() {
        return mUrl;
    }

    /** Whether the removal was performed through an action of the user. */
    public boolean isRequestedByUser() {
        return mRequestedByUser;
    }
}
