// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.net.Uri;
import android.text.TextUtils;

import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.components.url_formatter.UrlFormatter;

/** URL utilities used for touchless devices. **/
public class TouchlessUrlUtilities {
    /**
     * Formats a given URL for displaying in touchless. This formatting cuts the URL to its eTLD+1.
     * For example, for "https://chromium-review.googlesource.com/dashboard/", this will return
     * "googlesource.com".
     **/
    public static String getUrlForDisplay(String url) {
        if (TextUtils.isEmpty(url)) return url;

        String domainAndRegistry = UrlUtilities.getDomainAndRegistry(url, false);
        Uri parsed = Uri.parse(url);
        return UrlFormatter.formatUrlForSecurityDisplayOmitScheme(
                parsed.getScheme() + "://" + domainAndRegistry);
    }
}
