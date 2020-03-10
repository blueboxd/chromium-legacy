// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.net.Uri;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * Parameters for constructing context menu.
 */
public class ContextMenuParams {
    /**
     * The Uri associated with the main frame of the page that triggered the context menu.
     */
    @NonNull
    public final Uri pageUri;

    /**
     * The link Uri, if any.
     */
    @Nullable
    public final Uri linkUri;

    /**
     * The link text, if any.
     */
    @Nullable
    public final String linkText;

    /**
     * The title or alt attribute (if title is not available).
     */
    @Nullable
    public final String titleOrAltText;

    /**
     * This is the source Uri for the element that the context menu was
     * invoked on.  Example of elements with source URLs are img, audio, and
     * video.
     */
    @Nullable
    public final Uri srcUri;

    public ContextMenuParams(
            Uri pageUri, Uri linkUri, String linkText, String titleOrAltText, Uri srcUri) {
        this.pageUri = pageUri;
        this.linkUri = linkUri;
        this.linkText = linkText;
        this.titleOrAltText = titleOrAltText;
        this.srcUri = srcUri;
    }
}
