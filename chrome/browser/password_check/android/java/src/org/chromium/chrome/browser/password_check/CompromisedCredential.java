// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.url.GURL;

import java.util.Objects;

/**
 * This class holds the data used to represent a compromised credential in the Password Check
 * settings screen.
 */
public class CompromisedCredential {
    private final String mSignonRealm;
    private final GURL mOrigin;
    private final String mUsername;
    private final String mDisplayOrigin;
    private final String mDisplayUsername;
    private final String mPassword;
    private final String mPasswordChangeUrl;
    private final String mAssociatedApp;
    private final boolean mPhished;
    private final boolean mHasScript;

    /**
     * @param signonRealm The URL leading to the sign-on page.
     * @param origin The origin used to identify this credential (may be empty).
     * @param username The name used to identify this credential (may be empty).
     * @param displayOrigin The origin displayed to the user. Not necessarily a valid URL (e.g.
     *         missing scheme).
     * @param displayUsername The username displayed to the user (substituted if empty).
     * @param password The compromised password.
     * @param passwordChangeUrl A URL that links to the password change form of the affected site.
     * @param associatedApp The associated app if the password originates from it.
     * @param phished True iff the credential was entered on an unsafe site.
     * @param hasScript True iff the credential can be automatically fixed.
     */
    public CompromisedCredential(String signonRealm, GURL origin, String username,
            String displayOrigin, String displayUsername, String password, String passwordChangeUrl,
            String associatedApp, boolean phished, boolean hasScript) {
        assert origin != null : "Credential origin is null! Pass an empty one instead.";
        assert signonRealm != null;
        assert passwordChangeUrl != null : "Change URL may be empty but not null!";
        assert associatedApp != null : "App package name may be empty but not null!";
        assert !passwordChangeUrl.isEmpty()
                || !associatedApp.isEmpty()
            : "Change URL and app name may not be empty at the same time!";
        mSignonRealm = signonRealm;
        mOrigin = origin;
        mUsername = username;
        mDisplayOrigin = displayOrigin;
        mDisplayUsername = displayUsername;
        mPassword = password;
        mPasswordChangeUrl = passwordChangeUrl;
        mAssociatedApp = associatedApp;
        mPhished = phished;
        mHasScript = hasScript;
    }

    @CalledByNative
    public String getSignonRealm() {
        return mSignonRealm;
    }
    @CalledByNative
    public String getUsername() {
        return mUsername;
    }
    @CalledByNative
    public GURL getOrigin() {
        return mOrigin;
    }
    @CalledByNative
    public String getPassword() {
        return mPassword;
    }
    public String getDisplayUsername() {
        return mDisplayUsername;
    }
    public String getDisplayOrigin() {
        return mDisplayOrigin;
    }
    public boolean isPhished() {
        return mPhished;
    }
    public String getAssociatedApp() {
        return mAssociatedApp;
    }

    public String getPasswordChangeUrl() {
        return mPasswordChangeUrl;
    }
    public boolean hasScript() {
        return mHasScript;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;
        CompromisedCredential that = (CompromisedCredential) o;
        return mSignonRealm.equals(that.mSignonRealm) && mOrigin.equals(that.mOrigin)
                && mUsername.equals(that.mUsername) && mDisplayOrigin.equals(that.mDisplayOrigin)
                && mDisplayUsername.equals(that.mDisplayUsername)
                && mPassword.equals(that.mPassword)
                && mPasswordChangeUrl.equals(that.mPasswordChangeUrl)
                && mAssociatedApp.equals(that.mAssociatedApp) && mPhished == that.mPhished
                && mHasScript == that.mHasScript;
    }

    @Override
    public String toString() {
        return "CompromisedCredential{"
                + "signonRealm='" + mSignonRealm + ", origin='" + mOrigin + '\'' + '\''
                + ", username='" + mUsername + '\'' + ", displayOrigin='" + mDisplayOrigin + '\''
                + ", displayUsername='" + mDisplayUsername + '\'' + ", password='" + mPassword
                + '\'' + ", passwordChangeUrl='" + mPasswordChangeUrl + '\'' + ", associatedApp='"
                + mAssociatedApp + '\'' + ", phished=" + mPhished + ", hasScript=" + mHasScript
                + '}';
    }

    @Override
    public int hashCode() {
        return Objects.hash(mSignonRealm, mOrigin, mUsername, mDisplayOrigin, mDisplayUsername,
                mPassword, mPasswordChangeUrl, mAssociatedApp, mPhished, mHasScript);
    }
}
