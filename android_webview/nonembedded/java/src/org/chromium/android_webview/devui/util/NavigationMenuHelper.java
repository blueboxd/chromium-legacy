// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.devui.util;

import android.app.Activity;
import android.content.Intent;
import android.os.Build;
import android.provider.Settings;
import android.view.Menu;
import android.view.MenuItem;

import androidx.fragment.app.FragmentActivity;

import org.chromium.android_webview.devui.R;

/**
 * Helper class for menu to access external tools. Built-in tools should be handled by Fragments in
 * the MainActivity.
 */
public final class NavigationMenuHelper {
    /**
     * Inflate the navigation menu in the given {@code activity} options menu.
     *
     * This should be called inside {@link Activity#onCreateOptionsMenu} or
     * {@link android.support.v4.app.Fragment#onCreateOptionsMenu}.
     */
    public static void inflate(Activity activity, Menu menu) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) {
            return;
        }
        activity.getMenuInflater().inflate(R.menu.navigation_menu, menu);
    }

    /**
     * Handle {@code onOptionsItemSelected} event for navigation menu.
     *
     * This should be called inside {@code Activity#onOptionsItemSelected} method.
     * @return {@code true} if the item selection event is consumed.
     */
    public static boolean onOptionsItemSelected(FragmentActivity activity, MenuItem item) {
        if (item.getItemId() == R.id.nav_menu_switch_provider
                && Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            activity.startActivity(new Intent(Settings.ACTION_WEBVIEW_SETTINGS));
            return true;
        }
        return false;
    }

    // Do not instantiate this class.
    private NavigationMenuHelper() {}
}
