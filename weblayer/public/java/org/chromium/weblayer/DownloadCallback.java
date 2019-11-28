// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.net.Uri;

import androidx.annotation.NonNull;

/**
 * An interface that allows clients to handle download requests originating in the browser.
 */
public abstract class DownloadCallback {
    /**
     * A download of has been requested with the specified details. If it returns true the download
     * will be considered intercepted and WebLayer won't proceed with it. Note that there are many
     * corner cases where the embedder downloading it won't work (e.g. POSTs, one-time URLs,
     * requests that depend on cookies or auth state). If the method returns false, then currently
     * WebLayer won't download it but in the future this will be hooked up with new callbacks in
     * this interface.
     *
     * @param uri the target that should be downloaded
     * @param userAgent the user agent to be used for the download
     * @param contentDisposition content-disposition http header, if present
     * @param mimetype the mimetype of the content reported by the server
     * @param contentLength the file size reported by the server
     */
    public abstract boolean onInterceptDownload(@NonNull Uri uri, @NonNull String userAgent,
            @NonNull String contentDisposition, @NonNull String mimetype, long contentLength);
}
