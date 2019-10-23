// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.net.Uri;

import androidx.annotation.NonNull;

/**
 * Informed of interesting events that happen during the lifetime of a BrowserController.
 */
public abstract class BrowserObserver {
    /**
     * The Uri that should be displayed in the url-bar has updated.
     *
     * @param url The new user-visible url.
     */
    public void visibleUrlChanged(@NonNull Uri url) {}

    /**
     * The load state of the document has changed.
     *
     * @param isLoading Whether any resource is loading.
     * @param toDifferentDocument True if the main frame is loading a different document. Only valid
     *        when |isLoading| is true.
     */
    public void loadingStateChanged(boolean isLoading, boolean toDifferentDocument) {}

    /**
     * The progress of loading the main frame in the document has changed.
     *
     * @param progress A value in the range of 0.0-1.0.
     */
    public void loadProgressChanged(double progress) {}
}
