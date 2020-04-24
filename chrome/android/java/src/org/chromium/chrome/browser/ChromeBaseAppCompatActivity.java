// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.os.Bundle;

import androidx.annotation.CallSuper;
import androidx.annotation.Nullable;
import androidx.annotation.StyleRes;
import androidx.appcompat.app.AppCompatActivity;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.night_mode.GlobalNightModeStateProviderHolder;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.night_mode.NightModeUtils;

/**
 * A subclass of {@link AppCompatActivity} that maintains states applied to all activities in
 * {@link ChromeApplication} (e.g. night mode).
 */
public class ChromeBaseAppCompatActivity
        extends AppCompatActivity implements NightModeStateProvider.Observer {
    private NightModeStateProvider mNightModeStateProvider;
    private @StyleRes int mThemeResId;

    @Override
    protected void attachBaseContext(Context baseContext) {
        mNightModeStateProvider = createNightModeStateProvider();
        // Pre-Android O, fontScale gets initialized to 1 in the constructor. Set it to 0 so
        // that it is not interpreted as an overridden value.
        // https://crbug.com/834191
        Configuration overrideConfig = new Configuration();
        overrideConfig.fontScale = 0;
        applyConfigurationOverrides(baseContext, overrideConfig);

        super.attachBaseContext(baseContext.createConfigurationContext(overrideConfig));
    }

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        initializeNightModeStateProvider();
        mNightModeStateProvider.addObserver(this);
        super.onCreate(savedInstanceState);
    }

    @Override
    protected void onDestroy() {
        mNightModeStateProvider.removeObserver(this);
        super.onDestroy();
    }

    @Override
    public void setTheme(@StyleRes int resid) {
        super.setTheme(resid);
        mThemeResId = resid;
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        NightModeUtils.updateConfigurationForNightMode(this, newConfig, mThemeResId);
    }

    /**
     * Called during {@link #attachBaseContext(Context)} to allow for configuration overrides.
     * @param baseContext The base {@link Context} attached to this class.
     * @return A Configuration object with overrides set.
     */
    @CallSuper
    protected void applyConfigurationOverrides(Context baseContext, Configuration overrideConfig) {
        NightModeUtils.applyOverridesForNightMode(mNightModeStateProvider, overrideConfig);
    }

    /**
     * @return The {@link NightModeStateProvider} that provides the state of night mode.
     */
    public final NightModeStateProvider getNightModeStateProvider() {
        return mNightModeStateProvider;
    }

    /**
     * @return The {@link NightModeStateProvider} that provides the state of night mode in the scope
     *         of this class.
     */
    protected NightModeStateProvider createNightModeStateProvider() {
        return GlobalNightModeStateProviderHolder.getInstance();
    }

    /**
     * Initializes the initial night mode state. This will be called at the beginning of
     * {@link #onCreate(Bundle)} so that the correct theme can be applied for the initial night mode
     * state.
     */
    protected void initializeNightModeStateProvider() {}

    // NightModeStateProvider.Observer implementation.
    @Override
    public void onNightModeStateChanged() {
        if (!isFinishing()) recreate();
    }

    /**
     * Required to make preference fragments use InMemorySharedPreferences in tests.
     */
    @Override
    public SharedPreferences getSharedPreferences(String name, int mode) {
        return ContextUtils.getApplicationContext().getSharedPreferences(name, mode);
    }
}
