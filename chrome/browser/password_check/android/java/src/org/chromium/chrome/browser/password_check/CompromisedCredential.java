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
    private final String mPassword;
    private final String mOriginUrl;
    private final boolean mPhished;

    /**
     * @param username Username shown to the user.
     * @param originUrl Origin URL shown to the user in case this credential is a PSL match.
     */
    public CompromisedCredential(
            String originUrl, String username, String password, boolean phished) {
        assert originUrl != null : "Credential origin is null! Pass an empty one instead.";
        mOriginUrl = originUrl;
        mUsername = username;
        mPassword = password;
        mPhished = phished;
    }

    public String getOriginUrl() {
        return mOriginUrl;
    }
    public String getUsername() {
        return mUsername;
    }
    public String getPassword() {
        return mPassword;
    }
    public boolean isPhished() {
        return mPhished;
    }
}
