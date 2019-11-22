// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.api.client.knowncontent;

/** Information on the removal of a piece of content. */
public final class ContentRemoval {
    private final String url;
    private final boolean requestedByUser;

    public ContentRemoval(String url, boolean requestedByUser) {
        this.url = url;
        this.requestedByUser = requestedByUser;
    }

    /** Url for removed content. */
    public String getUrl() {
        return url;
    }

    /** Whether the removal was performed through an action of the user. */
    public boolean isRequestedByUser() {
        return requestedByUser;
    }
}
