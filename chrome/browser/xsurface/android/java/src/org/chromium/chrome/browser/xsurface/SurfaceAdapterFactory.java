// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

import android.content.Context;

/**
 * Creates SurfaceAdapters on demand.
 */
public interface SurfaceAdapterFactory {
    /**
     * Creates a new adapter backed by a shared set of dependencies with other adapters.
     * @param context The context that any new Android UI objects should be created within.
     * @return A new wrapper capable of making view objects.
     */
    SurfaceAdapter createSurfaceAdapter(Context context);
}
