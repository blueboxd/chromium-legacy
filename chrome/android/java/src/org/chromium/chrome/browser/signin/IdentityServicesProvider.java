// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.signin.AccountTrackerService;
import org.chromium.components.signin.identitymanager.IdentityManager;

/**
 * Provides access to sign-in related services that are profile-keyed on the native side. Java
 * equivalent of AccountTrackerServiceFactory and similar classes.
 */
public class IdentityServicesProvider {
    private static IdentityServicesProvider sIdentityServicesProvider;

    private IdentityServicesProvider() {}

    public static IdentityServicesProvider get() {
        if (sIdentityServicesProvider == null) {
            sIdentityServicesProvider = new IdentityServicesProvider();
        }
        return sIdentityServicesProvider;
    }

    @VisibleForTesting
    public static void setInstanceForTests(IdentityServicesProvider provider) {
        sIdentityServicesProvider = provider;
    }

    /** Getter for {@link IdentityManager} instance. */
    public IdentityManager getIdentityManager() {
        ThreadUtils.assertOnUiThread();
        // TODO(https://crbug.com/1041781): Use the current profile (i.e., regular profile or
        // incognito profile) instead of always using regular profile.
        IdentityManager result = IdentityServicesProviderJni.get().getIdentityManager(
                Profile.getLastUsedRegularProfile());
        assert result != null;
        return result;
    }

    /** Getter for {@link AccountTrackerService} instance. */
    public AccountTrackerService getAccountTrackerService() {
        ThreadUtils.assertOnUiThread();
        // TODO(https://crbug.com/1041781): Use the current profile (i.e., regular profile or
        // incognito profile) instead of always using regular profile.
        AccountTrackerService result = IdentityServicesProviderJni.get().getAccountTrackerService(
                Profile.getLastUsedRegularProfile());
        assert result != null;
        return result;
    }

    /** Getter for {@link SigninManager} instance. */
    public SigninManager getSigninManager() {
        ThreadUtils.assertOnUiThread();
        // TODO(https://crbug.com/1041781): Use the current profile (i.e., regular profile or
        // incognito profile) instead of always using regular profile.
        SigninManager result = IdentityServicesProviderJni.get().getSigninManager(
                Profile.getLastUsedRegularProfile());
        assert result != null;
        return result;
    }

    @NativeMethods
    interface Natives {
        IdentityManager getIdentityManager(Profile profile);
        AccountTrackerService getAccountTrackerService(Profile profile);
        SigninManager getSigninManager(Profile profile);
    }
}
