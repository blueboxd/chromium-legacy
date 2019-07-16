// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.IntDef;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;

import org.chromium.base.Log;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.preferences.ManagedPreferencesUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Allows user to pick an account and sign in. Started from Settings and various sign-in promos.
 */
// TODO(https://crbug.com/820491): extend AsyncInitializationActivity.
public class SigninActivity extends ChromeBaseAppCompatActivity {
    private static final String TAG = "SigninActivity";
    private static final String ARGUMENT_FRAGMENT_ARGS = "SigninActivity.FragmentArgs";

    @IntDef({SigninAccessPoint.SETTINGS, SigninAccessPoint.BOOKMARK_MANAGER,
            SigninAccessPoint.RECENT_TABS, SigninAccessPoint.SIGNIN_PROMO,
            SigninAccessPoint.NTP_CONTENT_SUGGESTIONS, SigninAccessPoint.AUTOFILL_DROPDOWN})
    @Retention(RetentionPolicy.SOURCE)
    public @interface AccessPoint {}

    /**
     * Creates an {@link Intent} which can be used to start sign-in flow.
     * @param accessPoint {@link AccessPoint} for starting sign-in flow. Used in metrics.
     */
    public static Intent createIntent(Context context, @AccessPoint int accessPoint) {
        return createIntentInternal(context, SigninFragment.createArguments(accessPoint));
    }

    /**
     * Creates an argument bundle to start default sign-in flow from personalized sign-in promo.
     * @param accessPoint The access point for starting sign-in flow.
     * @param accountName The account to preselect or null to preselect the default account.
     */
    public static Intent createIntentForPromoDefaultFlow(
            Context context, @SigninAccessPoint int accessPoint, String accountName) {
        return createIntentInternal(context,
                SigninFragment.createArgumentsForPromoDefaultFlow(accessPoint, accountName));
    }

    /**
     * Creates an argument bundle to start "Choose account" sign-in flow from personalized sign-in
     * promo.
     * @param accessPoint The access point for starting sign-in flow.
     * @param accountName The account to preselect or null to preselect the default account.
     */
    public static Intent createIntentForPromoChooseAccountFlow(
            Context context, @SigninAccessPoint int accessPoint, String accountName) {
        return createIntentInternal(context,
                SigninFragment.createArgumentsForPromoChooseAccountFlow(accessPoint, accountName));
    }

    /**
     * Creates an argument bundle to start "New account" sign-in flow from personalized sign-in
     * promo.
     * @param accessPoint The access point for starting sign-in flow.
     */
    public static Intent createIntentForPromoAddAccountFlow(
            Context context, @SigninAccessPoint int accessPoint) {
        return createIntentInternal(
                context, SigninFragment.createArgumentsForPromoAddAccountFlow(accessPoint));
    }

    private static Intent createIntentInternal(Context context, Bundle fragmentArgs) {
        Intent intent = new Intent(context, SigninActivity.class);
        intent.putExtra(ARGUMENT_FRAGMENT_ARGS, fragmentArgs);
        return intent;
    }

    /**
     * A convenience method to create a SigninActivity passing the access point in the
     * intent. Checks if the sign in flow can be started before showing the activity.
     * @param accessPoint {@link AccessPoint} for starting signin flow. Used in metrics.
     * @return {@code true} if sign in has been allowed.
     */
    public static boolean startIfAllowed(Context context, @AccessPoint int accessPoint) {
        SigninManager signinManager = IdentityServicesProvider.getSigninManager();
        if (!signinManager.isSignInAllowed()) {
            if (signinManager.isSigninDisabledByPolicy()) {
                ManagedPreferencesUtils.showManagedByAdministratorToast(context);
            }
            return false;
        }

        context.startActivity(createIntent(context, accessPoint));
        return true;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Make sure the native is initialized before calling super.onCreate(), as it might recreate
        // SigninFragment that currently depends on native. See https://crbug.com/983730.
        try {
            ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
        } catch (ProcessInitException e) {
            Log.e(TAG, "Failed to start browser process.", e);
            ChromeApplication.reportStartupErrorAndExit(e);
        }

        super.onCreate(savedInstanceState);
        setContentView(R.layout.signin_activity);

        FragmentManager fragmentManager = getSupportFragmentManager();
        Fragment fragment = fragmentManager.findFragmentById(R.id.fragment_container);
        if (fragment == null) {
            Bundle fragmentArgs = getIntent().getBundleExtra(ARGUMENT_FRAGMENT_ARGS);
            fragment = Fragment.instantiate(this, SigninFragment.class.getName(), fragmentArgs);
            fragmentManager.beginTransaction().add(R.id.fragment_container, fragment).commit();
        }
    }
}
