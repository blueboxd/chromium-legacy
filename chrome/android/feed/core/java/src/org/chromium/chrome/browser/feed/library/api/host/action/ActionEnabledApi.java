// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.action;

/** Allows the Feed to query the host as to what actions are enabled. */
public interface ActionEnabledApi {
    /** Whether the host can open a URL. */
    boolean canOpenUrl();

    /** Whether the host can open a URL in incognito mode. */
    boolean canOpenUrlInIncognitoMode();

    /** Whether the host can open a URL in a new tab. */
    boolean canOpenUrlInNewTab();

    /** Whether the host can open a URL in a new window. */
    boolean canOpenUrlInNewWindow();

    /** Whether the host can download a URL. */
    boolean canDownloadUrl();

    /** Whether the host can open the Google Product Help page. */
    boolean canLearnMore();
}
