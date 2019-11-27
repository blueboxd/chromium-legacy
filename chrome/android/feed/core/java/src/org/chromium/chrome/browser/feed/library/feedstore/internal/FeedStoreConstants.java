// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedstore.internal;

/** Group of constants for use across the {@link FeedStore} class and its internal classes */
public final class FeedStoreConstants {
    /** Can't be instantiated */
    private FeedStoreConstants() {}

    /** Used to prefix session IDs */
    public static final String SESSION_NAME_PREFIX = "_session:";

    /** Key used to prefix Feed shared states keys in {@link ContentStorage} */
    public static final String SHARED_STATE_PREFIX = "ss::";

    /** Key used to prefix Feed semantic properties keys in {@link ContentStorage} */
    public static final String SEMANTIC_PROPERTIES_PREFIX = "sp::";

    /** Key used to prefix Feed uploadableAction keys in {@link ContentStorage} */
    public static final String UPLOADABLE_ACTION_PREFIX = "ua::";

    /** The name of the journal used to store dismiss actions */
    public static final String DISMISS_ACTION_JOURNAL = "action-dismiss";
}
