// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.net.Uri;

import androidx.annotation.NonNull;

/**
 * Informed of interesting events that happen during the lifetime of a Tab.
 */
public abstract class TabCallback {
    /**
     * The Uri that should be displayed in the location-bar has updated.
     *
     * @param uri The new user-visible uri.
     */
    public void onVisibleUriChanged(@NonNull Uri uri) {}

    /**
     * Triggered when the render process dies, either due to crash or killed by the system to
     * reclaim memory.
     */
    public void onRenderProcessGone() {}
}
