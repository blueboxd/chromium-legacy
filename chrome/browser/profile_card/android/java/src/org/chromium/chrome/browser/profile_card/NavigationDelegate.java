// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profile_card;

/** A delegate that navigate the page according to the provided url. */
public interface NavigationDelegate {
    /**
     * @param The {@link String} url to navigate to.
     */
    public void navigateTo(String url);
}
