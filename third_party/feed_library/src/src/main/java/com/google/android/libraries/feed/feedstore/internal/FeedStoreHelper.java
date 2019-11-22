// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedstore.internal;

import static com.google.android.libraries.feed.feedstore.internal.FeedStoreConstants.SESSION_NAME_PREFIX;

import java.util.UUID;

/** Helper class for shared FeedStore methods */
public final class FeedStoreHelper {
    /** Get a new, unique stream session id. */
    String getNewStreamSessionId() {
        return SESSION_NAME_PREFIX + UUID.randomUUID();
    }
}
