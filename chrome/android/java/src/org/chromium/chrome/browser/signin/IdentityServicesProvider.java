// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.signin.AccountTrackerService;
import org.chromium.components.signin.OAuth2TokenService;

/**
 * Provides access to sign-in related services that are profile-keyed on the native side. Java
 * equivalent of AccountTrackerServiceFactory and similar classes.
 */
public final class IdentityServicesProvider {
    /** Getter for {@link AccountTrackerService} instance. */
    public static AccountTrackerService getAccountTrackerService() {
        ThreadUtils.assertOnUiThread();
        AccountTrackerService result = nativeGetAccountTrackerService(Profile.getLastUsedProfile());
        assert result != null;
        return result;
    }

    public static OAuth2TokenService getOAuth2TokenService() {
        ThreadUtils.assertOnUiThread();
        OAuth2TokenService result = nativeGetOAuth2TokenService(Profile.getLastUsedProfile());
        assert result != null;
        return result;
    }

    public static SigninManager getSigninManager() {
        ThreadUtils.assertOnUiThread();
        SigninManager result = nativeGetSigninManager(Profile.getLastUsedProfile());
        assert result != null;
        return result;
    }

    private static native AccountTrackerService nativeGetAccountTrackerService(Profile profile);
    private static native OAuth2TokenService nativeGetOAuth2TokenService(Profile profile);
    private static native SigninManager nativeGetSigninManager(Profile profile);
}
