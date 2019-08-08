// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Intent;
import android.support.annotation.NonNull;
import android.support.v7.app.AppCompatDelegate;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.night_mode.SystemNightModeMonitor;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.util.IntentUtils;

import androidx.browser.customtabs.CustomTabsIntent;

/**
 * Maintains and provides the night mode state for {@link CustomTabActivity}.
 */
public class CustomTabNightModeStateController
        implements Destroyable, NightModeStateProvider, SystemNightModeMonitor.Observer {
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    /**
     * The color scheme requested for the CCT. Only {@link CustomTabsIntent#COLOR_SCHEME_LIGHT}
     * and {@link CustomTabsIntent#COLOR_SCHEME_DARK} should be considered - fall back to the
     * system status for {@link CustomTabsIntent#COLOR_SCHEME_SYSTEM} when enabled.
     */
    private int mRequestedColorScheme;
    private AppCompatDelegate mAppCompatDelegate;

    /**
     * @param lifecycleDispatcher The {@link ActivityLifecycleDispatcher} that will notify this
     *                            class about lifecycle changes.
     */
    CustomTabNightModeStateController(ActivityLifecycleDispatcher lifecycleDispatcher) {
        lifecycleDispatcher.register(this);
    }

    /**
     * Initializes the initial night mode state.
     * @param delegate The {@link AppCompatDelegate} that controls night mode state in support
     *                 library.
     * @param intent  The {@link Intent} to retrieve information about the initial state.
     */
    void initialize(AppCompatDelegate delegate, Intent intent) {
        if (!NightModeUtils.isNightModeSupported()
                || !FeatureUtilities.isNightModeForCustomTabsAvailable()) {
            // Always stay in light mode if night mode is not available.
            mRequestedColorScheme = CustomTabsIntent.COLOR_SCHEME_LIGHT;
            return;
        }

        mRequestedColorScheme = IntentUtils.safeGetIntExtra(
                intent, CustomTabsIntent.EXTRA_COLOR_SCHEME, CustomTabsIntent.COLOR_SCHEME_SYSTEM);
        mAppCompatDelegate = delegate;

        updateNightMode();

        // No need to observe system night mode if the intent specifies a light/dark color scheme.
        if (mRequestedColorScheme == CustomTabsIntent.COLOR_SCHEME_SYSTEM) {
            SystemNightModeMonitor.getInstance().addObserver(this);
        }
    }

    // Destroyable implementation.
    @Override
    public void destroy() {
        SystemNightModeMonitor.getInstance().removeObserver(this);
    }

    // NightModeStateProvider implementation.
    @Override
    public boolean isInNightMode() {
        switch (mRequestedColorScheme) {
            case CustomTabsIntent.COLOR_SCHEME_LIGHT:
                return false;
            case CustomTabsIntent.COLOR_SCHEME_DARK:
                return true;
            default:
                return SystemNightModeMonitor.getInstance().isSystemNightModeOn();
        }
    }

    @Override
    public void addObserver(@NonNull Observer observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(@NonNull Observer observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public boolean shouldOverrideConfiguration() {
        // Don't override configuration because the initial night mode state is only available
        // during CustomTabActivity#onCreate().
        return false;
    }

    // SystemNightModeMonitor.Observer implementation.
    @Override
    public void onSystemNightModeChanged() {
        updateNightMode();
        // We need to notify observers on system night mode change so that activities can be
        // restarted as needed (we do not handle color scheme changes during runtime). No need to
        // check for color scheme because we don't observe system night mode for light/dark color
        // scheme.
        for (Observer observer : mObservers) observer.onNightModeStateChanged();
    }

    private void updateNightMode() {
        mAppCompatDelegate.setLocalNightMode(isInNightMode() ? AppCompatDelegate.MODE_NIGHT_YES
                                                             : AppCompatDelegate.MODE_NIGHT_NO);
    }
}
