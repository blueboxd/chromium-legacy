// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.url.GURL;

/** URL utilities. */
public class UrlUtils {
    /**
     * Returns false for invalid URL format or a relative URI.
     *
     * @param url The payment method name.
     * @return TRUE if given url is valid and not a relative URI.
     */
    public static boolean isURLValid(GURL url) {
        return url != null && url.isValid() && !url.getScheme().isEmpty()
                && (UrlConstants.HTTPS_SCHEME.equals(url.getScheme())
                        || UrlConstants.HTTP_SCHEME.equals(url.getScheme()));
    }

    private UrlUtils() {}
}
