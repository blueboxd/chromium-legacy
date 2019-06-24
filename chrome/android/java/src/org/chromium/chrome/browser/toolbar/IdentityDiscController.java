// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.support.annotation.DimenRes;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.SyncAndServicesPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.ProfileDataCache;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.signin.ChromeSigninController;

import java.util.Collections;

/**
 * Handles displaying IdentityDisc on toolbar depending on several conditions
 * (user sign-in state, whether NTP is shown)
 */
class IdentityDiscController implements NativeInitObserver, ProfileDataCache.Observer,
                                        SigninManager.SignInStateObserver,
                                        ProfileSyncService.SyncStateChangedListener {
    // Context is used for fetching resources and launching preferences page.
    private final Context mContext;
    // Toolbar manager exposes APIs for manipulating experimental button.
    private final ToolbarManager mToolbarManager;
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    // SigninManager and ProfileSyncService allow observing sign-in and sync state.
    private SigninManager mSigninManager;
    private ProfileSyncService mProfileSyncService;

    // ProfileDataCache facilitates retrieving profile picture.
    private ProfileDataCache mProfileDataCache;

    private boolean mIsIdentityDiscVisible;
    private boolean mIsNTPVisible;

    /**
     * Creates IdentityDiscController object.
     * @param context The Context for retrieving resources, launching preference activiy, etc.
     * @param toolbarManager The ToolbarManager where Identity Disc is displayed.
     */
    IdentityDiscController(Context context, ToolbarManager toolbarManager,
            ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        mContext = context;
        mToolbarManager = toolbarManager;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);
    }

    /**
     * Registers itself to observe sign-in and sync status events.
     */
    @Override
    public void onFinishNativeInitialization() {
        mActivityLifecycleDispatcher.unregister(this);
        mActivityLifecycleDispatcher = null;

        mProfileSyncService = ProfileSyncService.get();
        // ProfileSyncService being null means sync is disabled and won't be initialized. This means
        // Identity Disc will never get shown so we don't need to register for other notifications.
        if (mProfileSyncService == null) return;

        mProfileSyncService.addSyncStateChangedListener(this);

        mSigninManager = SigninManager.get();
        mSigninManager.addSignInStateObserver(this);
    }

    /**
     * Shows/hides Identity Disc depending on whether NTP is visible.
     */
    void updateButtonState(boolean isNTPVisible) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.IDENTITY_DISC)) return;
        // Sync is disabled. IdentityDisc will never be shown.
        if (mProfileSyncService == null) return;

        mIsNTPVisible = isNTPVisible;
        String accountName = ChromeSigninController.get().getSignedInAccountName();
        boolean shouldShowIdentityDisc =
                isNTPVisible && accountName != null && mProfileSyncService.canSyncFeatureStart();

        if (shouldShowIdentityDisc == mIsIdentityDiscVisible) return;

        if (shouldShowIdentityDisc) {
            mIsIdentityDiscVisible = true;
            createProfileDataCache(accountName);
            showIdentityDisc(accountName);
            maybeShowIPH();
        } else {
            mIsIdentityDiscVisible = false;
            mToolbarManager.disableExperimentalButton();
        }
    }

    /**
     * Creates and initializes ProfileDataCache if it wasn't created previously. Subscribes
     * IdentityDiscController for profile data updates.
     */
    private void createProfileDataCache(String accountName) {
        if (mProfileDataCache != null) return;

        @DimenRes
        int dimension_id =
                mToolbarManager.isBottomToolbarVisible() ? R.dimen.toolbar_identity_disc_size_duet
                                                         : R.dimen.toolbar_identity_disc_size;
        int imageSize = mContext.getResources().getDimensionPixelSize(dimension_id);
        mProfileDataCache = new ProfileDataCache(mContext, imageSize);
        mProfileDataCache.addObserver(this);
        mProfileDataCache.update(Collections.singletonList(accountName));
    }

    /**
     * Triggers profile image fetch and displays Identity Disc on top toolbar.
     */
    private void showIdentityDisc(String accountName) {
        Drawable profileImage = mProfileDataCache.getProfileDataOrDefault(accountName).getImage();
        mToolbarManager.enableExperimentalButton(view -> {
            RecordUserAction.record("MobileToolbarIdentityDiscTap");
            PreferencesLauncher.launchSettingsPage(mContext, SyncAndServicesPreferences.class);
        }, profileImage, R.string.accessibility_toolbar_btn_identity_disc);
    }

    /**
     * Called after profile image becomes available. Updates the image on toolbar button.
     */
    @Override
    public void onProfileDataUpdated(String accountId) {
        assert mProfileDataCache != null;
        if (!mIsIdentityDiscVisible) return;

        String accountName = ChromeSigninController.get().getSignedInAccountName();
        if (accountId.equals(accountName)) {
            Drawable image = mProfileDataCache.getProfileDataOrDefault(accountName).getImage();
            mToolbarManager.updateExperimentalButtonImage(image);
        }
    }

    // SigninManager.SignInStateObserver implementation.
    @Override
    public void onSignedIn() {
        String accountName = ChromeSigninController.get().getSignedInAccountName();
        if (mProfileDataCache != null && accountName != null) {
            mProfileDataCache.update(Collections.singletonList(accountName));
        }
        updateButtonState(mIsNTPVisible);
    }

    @Override
    public void onSignedOut() {
        updateButtonState(mIsNTPVisible);
    }

    // ProfileSyncService.SyncStateChangedListener implementation.
    @Override
    public void syncStateChanged() {
        updateButtonState(mIsNTPVisible);
    }

    /**
     * Call to tear down dependencies.
     */
    void destroy() {
        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.unregister(this);
            mActivityLifecycleDispatcher = null;
        }

        if (mProfileDataCache != null) {
            mProfileDataCache.removeObserver(this);
            mProfileDataCache = null;
        }
        if (mSigninManager != null) {
            mSigninManager.removeSignInStateObserver(this);
            mSigninManager = null;
        }
        if (mProfileSyncService != null) {
            mProfileSyncService.removeSyncStateChangedListener(this);
            mProfileSyncService = null;
        }
    }

    /**
     * Shows a help bubble below Identity Disc if the In-Product Help conditions are met.
     */
    private void maybeShowIPH() {
        Profile profile = Profile.getLastUsedProfile();
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        if (!tracker.shouldTriggerHelpUI(FeatureConstants.IDENTITY_DISC_FEATURE)) return;

        mToolbarManager.showIPHOnExperimentalButton(R.string.iph_identity_disc_text,
                R.string.iph_identity_disc_accessibility_text,
                () -> { tracker.dismissed(FeatureConstants.IDENTITY_DISC_FEATURE); });
    }
}
