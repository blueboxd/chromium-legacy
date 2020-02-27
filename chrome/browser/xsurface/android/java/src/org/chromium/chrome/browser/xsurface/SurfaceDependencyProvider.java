// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

import android.content.Context;

/**
 * Provides logging and context for an external surface.
 */
public interface SurfaceDependencyProvider {
    /** @return the context associated with the application. */
    Context getContext();

    /** @see {Log.e} */
    void logError(String tag, String messageTemplate, Object... args);

    /** @see {Log.w} */
    void logWarning(String tag, String messageTemplate, Object... args);
}
