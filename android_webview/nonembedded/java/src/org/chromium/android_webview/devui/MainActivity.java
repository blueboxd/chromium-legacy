// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.devui;

import android.content.Intent;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;

import org.chromium.base.ApiCompatibilityUtils;

import java.util.HashMap;
import java.util.Map;

/**
 * Dev UI main activity.
 * It shows persistent errors and helps to navigate to WebView developer tools.
 */
public class MainActivity extends FragmentActivity {
    private WebViewPackageError mDifferentPackageError;
    private boolean mSwitchFragmentOnResume;
    final Map<Integer, Integer> mFragmentIdMap = new HashMap<>();

    // Keep in sync with DeveloperUiService.java
    private static final String FRAGMENT_ID_INTENT_EXTRA = "fragment-id";
    private static final int FRAGMENT_ID_HOME = 0;
    private static final int FRAGMENT_ID_CRASHES = 1;
    private static final int FRAGMENT_ID_FLAGS = 2;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_main);

        // Let onResume handle showing the initial Fragment.
        mSwitchFragmentOnResume = true;

        mDifferentPackageError = new WebViewPackageError(this);
        // show the dialog once when the activity is created.
        mDifferentPackageError.showDialogIfDifferent();

        // Set up bottom navigation bar:
        mFragmentIdMap.put(R.id.navigation_home, FRAGMENT_ID_HOME);
        mFragmentIdMap.put(R.id.navigation_crash_ui, FRAGMENT_ID_CRASHES);
        mFragmentIdMap.put(R.id.navigation_flags_ui, FRAGMENT_ID_FLAGS);
        LinearLayout bottomNavBar = findViewById(R.id.nav_view);
        View.OnClickListener listener = (View view) -> {
            assert mFragmentIdMap.containsKey(view.getId()) : "Unexpected view ID: " + view.getId();
            int fragmentId = mFragmentIdMap.get(view.getId());
            switchFragment(fragmentId);
        };
        final int childCount = bottomNavBar.getChildCount();
        for (int i = 0; i < childCount; ++i) {
            View v = bottomNavBar.getChildAt(i);
            v.setOnClickListener(listener);
        }
    }

    private void switchFragment(int chosenFragmentId) {
        Fragment fragment = null;
        switch (chosenFragmentId) {
            default:
                chosenFragmentId = FRAGMENT_ID_HOME;
                // Fall through.
            case FRAGMENT_ID_HOME:
                fragment = new HomeFragment();
                break;
            case FRAGMENT_ID_CRASHES:
                fragment = new CrashesListFragment();
                break;
            case FRAGMENT_ID_FLAGS:
                fragment = new FlagsFragment();
                break;
        }
        assert fragment != null;

        // Switch fragments
        FragmentManager fm = getSupportFragmentManager();
        FragmentTransaction transaction = fm.beginTransaction();
        transaction.replace(R.id.content_fragment, fragment);
        transaction.commit();

        // Update the bottom toolbar
        LinearLayout bottomNavBar = findViewById(R.id.nav_view);
        final int childCount = bottomNavBar.getChildCount();
        for (int i = 0; i < childCount; ++i) {
            View view = bottomNavBar.getChildAt(i);
            assert mFragmentIdMap.containsKey(view.getId()) : "Unexpected view ID: " + view.getId();
            int fragmentId = mFragmentIdMap.get(view.getId());
            assert view instanceof TextView : "Bottom bar must have TextViews as direct children";
            TextView textView = (TextView) view;

            boolean isSelectedFragment = chosenFragmentId == fragmentId;
            ApiCompatibilityUtils.setTextAppearance(textView,
                    isSelectedFragment ? R.style.SelectedNavigationButton
                                       : R.style.UnselectedNavigationButton);
            int color = isSelectedFragment ? getResources().getColor(R.color.navigation_selected)
                                           : getResources().getColor(R.color.navigation_unselected);
            for (Drawable drawable : textView.getCompoundDrawables()) {
                if (drawable != null) {
                    drawable.setColorFilter(
                            new PorterDuffColorFilter(color, PorterDuff.Mode.SRC_IN));
                }
            }
        }
    }

    @Override
    protected void onNewIntent(Intent intent) {
        // Store the Intent so we can switch Fragments in onResume (which is called next). Only need
        // to switch Fragment if the Intent specifies to do so.
        setIntent(intent);
        mSwitchFragmentOnResume = intent.hasExtra(FRAGMENT_ID_INTENT_EXTRA);
    }

    @Override
    protected void onResume() {
        super.onResume();
        // Check package status in onResume() to hide/show the error message if the user
        // changes WebView implementation from system settings and then returns back to the
        // activity.
        mDifferentPackageError.showMessageIfDifferent();

        // Don't change Fragment unless we have a new Intent, since the user might just be coming
        // back to this through the task switcher.
        if (!mSwitchFragmentOnResume) return;

        // Ensure we only switch the first time we see a new Intent.
        mSwitchFragmentOnResume = false;

        // Default to HomeFragment if not specified.
        int fragmentId = FRAGMENT_ID_HOME;
        // FRAGMENT_ID_INTENT_EXTRA is an optional extra to specify which fragment to open. At the
        // moment, it's specified only by DeveloperUiService (so make sure these constants stay in
        // sync).
        Bundle extras = getIntent().getExtras();
        if (extras != null) {
            fragmentId = extras.getInt(FRAGMENT_ID_INTENT_EXTRA, fragmentId);
        }
        switchFragment(fragmentId);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            getMenuInflater().inflate(R.menu.navigation_menu, menu);
        }
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.nav_menu_switch_provider
                && Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            startActivity(new Intent(Settings.ACTION_WEBVIEW_SETTINGS));
            return true;
        }
        return super.onOptionsItemSelected(item);
    }
}
