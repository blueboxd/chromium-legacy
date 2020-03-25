// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.devui;

import android.content.Intent;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;

import org.chromium.android_webview.devui.util.NavigationMenuHelper;

/**
 * Dev UI main activity.
 * It shows persistent errors and helps to navigate to WebView developer tools.
 */
public class MainActivity extends FragmentActivity {
    private WebViewPackageError mDifferentPackageError;
    private boolean mSwitchFragmentOnResume;

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

        Fragment fragment = null;
        switch (fragmentId) {
            default:
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

        FragmentManager fm = getSupportFragmentManager();
        FragmentTransaction transaction = fm.beginTransaction();
        transaction.replace(R.id.content_fragment, fragment);
        transaction.commit();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        NavigationMenuHelper.inflate(this, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (NavigationMenuHelper.onOptionsItemSelected(this, item)) {
            return true;
        }
        return super.onOptionsItemSelected(item);
    }
}
