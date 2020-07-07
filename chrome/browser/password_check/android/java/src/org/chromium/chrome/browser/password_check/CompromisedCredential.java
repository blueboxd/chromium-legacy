// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

/**
 * This class holds the data used to represent a compromised credential in the Password Check
 * settings screen.
 */
public class CompromisedCredential {
    private final String mUsername;
    private final String mOriginUrl;

    /**
     * @param username Username shown to the user.
     * @param originUrl Origin URL shown to the user in case this credential is a PSL match.
     */
    public CompromisedCredential(String username, String originUrl) {
        assert originUrl != null : "Credential origin is null! Pass an empty one instead.";
        mUsername = username;
        mOriginUrl = originUrl;
    }

    public String getUsername() {
        return mUsername;
    }

    public String getOriginUrl() {
        return mOriginUrl;
    }
}
