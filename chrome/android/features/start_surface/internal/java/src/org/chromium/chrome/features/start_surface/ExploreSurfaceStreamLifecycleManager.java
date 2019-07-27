// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.app.Activity;

import com.google.android.libraries.feed.api.client.stream.Stream;

import org.chromium.chrome.browser.feed.StreamLifecycleManager;

/** Explore surface feed stream lifecycle manager. */
class ExploreSurfaceStreamLifecycleManager extends StreamLifecycleManager {
    /**
     * The constructor.
     * @param stream The {@link Stream} this manager manages.
     * @param activity The activity the {@link Stream} associates with.
     */
    ExploreSurfaceStreamLifecycleManager(Stream stream, Activity activity) {
        super(stream, activity);
        start();
    }

    // TODO(crbug.com/982018): Save and restore instance state when opening the feeds in normal
    // Tabs.
}