// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v7.widget.RecyclerView;
import android.view.Menu;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.touchless.dialog.TouchlessDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The Chrome settings activity for touchless devices.
 */
public class TouchlessPreferences extends Preferences {
    private TouchlessModelCoordinator mTouchlessModelCoordinator;
    private PropertyModel mDialogModel;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getSupportActionBar().setDisplayHomeAsUpEnabled(false);
        mTouchlessModelCoordinator = AppHooks.get().createTouchlessModelCoordinator(this);
        mDialogModel =
                new PropertyModel.Builder(TouchlessDialogProperties.MINIMAL_DIALOG_KEYS).build();
        if (mTouchlessModelCoordinator != null) {
            mTouchlessModelCoordinator.addModelToQueue(mDialogModel);
        }
    }

    /**
     * Adds paddings for the main list in the current support library Fragment.
     */
    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        Fragment fragment = getMainFragmentCompat();
        if (fragment == null || fragment.getView() == null
                || fragment.getView().findViewById(R.id.list) == null) {
            return;
        }

        int padding = getResources().getDimensionPixelSize(
                org.chromium.chrome.touchless.R.dimen.touchless_preferences_highlight_padding);
        RecyclerView recyclerView = fragment.getView().findViewById(R.id.list);
        recyclerView.setPadding(padding, 0, padding, 0);
    }

    @Override
    protected void onResume() {
        super.onResume();
        TouchlessDialogProperties.ActionNames actionNames =
                new TouchlessDialogProperties.ActionNames();
        actionNames.select = R.string.select;
        actionNames.cancel = R.string.back;
        actionNames.alt = 0;
        mDialogModel.set(TouchlessDialogProperties.ACTION_NAMES, actionNames);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        return false;
    }
}
