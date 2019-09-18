// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.addtohomescreen;

import android.app.Activity;
import android.graphics.Bitmap;
import android.os.Build;
import android.text.TextUtils;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;

/**
 * Manages the add to home screen process. Coordinates the native-side data fetching, and owns
 * a dialog prompting the user to confirm the action (and potentially supply a title).
 */
public class AddToHomescreenManager implements AddToHomescreenView.Delegate {
    protected final Activity mActivity;
    protected final Tab mTab;

    protected AddToHomescreenView mDialog;
    private long mNativeAddToHomescreenManager;

    public AddToHomescreenManager(Activity activity, Tab tab) {
        mActivity = activity;
        mTab = tab;
    }

    /**
     * Starts the add to home screen process. Creates the C++ AddToHomescreenManager, which fetches
     * the data needed for add to home screen, and informs this object when data is available and
     * when the dialog can be shown.
     */
    public void start() {
        // Don't start if we've already started or if there is no visible URL to add.
        if (mNativeAddToHomescreenManager != 0 || TextUtils.isEmpty(mTab.getUrl())) return;

        mNativeAddToHomescreenManager = AddToHomescreenManagerJni.get().initializeAndStart(
                AddToHomescreenManager.this, mTab.getWebContents());
    }

    /**
     * Puts the object in a state where it is safe to be destroyed.
     */
    public void destroy() {
        mDialog = null;
        if (mNativeAddToHomescreenManager == 0) return;

        AddToHomescreenManagerJni.get().destroy(
                mNativeAddToHomescreenManager, AddToHomescreenManager.this);
        mNativeAddToHomescreenManager = 0;
    }

    /**
     * Adds a shortcut for the current Tab. Must not be called unless start() has been called.
     * @param userRequestedTitle Title of the shortcut displayed on the homescreen.
     */
    @Override
    public void addToHomescreen(String userRequestedTitle) {
        assert mNativeAddToHomescreenManager != 0;

        AddToHomescreenManagerJni.get().addToHomescreen(
                mNativeAddToHomescreenManager, AddToHomescreenManager.this, userRequestedTitle);
    }

    @Override
    public void onNativeAppDetailsRequested() {
        // This should never be called.
        assert false;
    }

    @Override
    /**
     * Destroys this object once the dialog has been dismissed.
     */
    public void onDialogDismissed() {
        destroy();
    }

    /**
     * Shows alert to prompt user for name of home screen shortcut.
     */
    @CalledByNative
    public void showDialog() {
        mDialog = new AddToHomescreenView(mActivity, this);
        mDialog.show();
    }

    @CalledByNative
    private void onUserTitleAvailable(String title, String url, boolean isWebapp) {
        // Users may edit the title of bookmark shortcuts, but we respect web app names and do not
        // let users change them.
        mDialog.onUserTitleAvailable(title, url, isWebapp);
    }

    @CalledByNative
    private void onIconAvailable(Bitmap icon, boolean iconAdaptable) {
        if (iconAdaptable && Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            mDialog.onAdaptableIconAvailable(icon);
        } else {
            assert !iconAdaptable : "Adaptive icons should not be provided pre-Android O.";
            mDialog.onIconAvailable(icon);
        }
    }

    @NativeMethods
    interface Natives {
        long initializeAndStart(AddToHomescreenManager caller, WebContents webContents);
        void addToHomescreen(long nativeAddToHomescreenManager, AddToHomescreenManager caller,
                String userRequestedTitle);
        void destroy(long nativeAddToHomescreenManager, AddToHomescreenManager caller);
    }
}
