// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.config;

/**
 * Provides configuration details about the build, such as how much debug logging should occur.
 *
 * <p>Member booleans should control _behavior_ (turning debug features on or off), rather than
 * reporting a build state (like dev or release).
 */
// TODO: This can't be final because we mock it
public class DebugBehavior {
    /** Convenience constant for configuration that enables all debug behavior. */
    public static final DebugBehavior VERBOSE = new DebugBehavior(true);

    /** Convenience constant for configuration that disables all debug behavior. */
    public static final DebugBehavior SILENT = new DebugBehavior(false);

    private final boolean mShowDebugViews;

    private DebugBehavior(boolean showDebugViews) {
        this.mShowDebugViews = showDebugViews;
    }

    public boolean getShowDebugViews() {
        return mShowDebugViews;
    }
}
