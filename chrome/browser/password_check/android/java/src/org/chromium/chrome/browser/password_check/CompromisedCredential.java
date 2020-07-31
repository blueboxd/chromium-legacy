// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import java.util.Objects;

/**
 * This class holds the data used to represent a compromised credential in the Password Check
 * settings screen.
 */
public class CompromisedCredential {
    private final String mUsername;
    private final String mPassword;
    private final String mOriginUrl;
    private final boolean mPhished;
    private final boolean mHasScript;

    /**
     * @param username Username shown to the user.
     * @param originUrl Origin URL shown to the user in case this credential is a PSL match.
     */
    public CompromisedCredential(String originUrl, String username, String password,
            boolean phished, boolean hasScript) {
        assert originUrl != null : "Credential origin is null! Pass an empty one instead.";
        mOriginUrl = originUrl;
        mUsername = username;
        mPassword = password;
        mPhished = phished;
        mHasScript = hasScript;
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
    public boolean hasScript() {
        return mHasScript;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;
        CompromisedCredential that = (CompromisedCredential) o;
        return mPhished == that.mPhished && mHasScript == that.mHasScript
                && mUsername.equals(that.mUsername) && mPassword.equals(that.mPassword)
                && mOriginUrl.equals(that.mOriginUrl);
    }

    @Override
    public int hashCode() {
        return Objects.hash(mUsername, mPassword, mOriginUrl, mPhished, mHasScript);
    }
}
