// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.net.Uri;

import androidx.annotation.Nullable;

/**
 * @see org.chromium.components.embedder_support.util.Origin
 */
// TODO(crbug.com/1058597): Delete this class once Clank migrates to the embedder_support version.
public class Origin extends org.chromium.components.embedder_support.util.Origin {
    protected Origin(Uri origin) {
        super(origin);
    }

    @Nullable
    public static Origin create(org.chromium.components.embedder_support.util.Origin origin) {
        if (origin == null) {
            return null;
        }
        return new Origin(origin.uri());
    }

    /**
     * Constructs a canonical Origin from an Uri. Will return {@code null} for origins that are not
     * HTTP or HTTPS.
     */
    @Nullable
    public static Origin create(Uri uri) {
        return create(org.chromium.components.embedder_support.util.Origin.create(uri));
    }
}
