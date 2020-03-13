// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth.authenticator;

import android.Manifest.permission;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;

import androidx.fragment.app.Fragment;

import org.chromium.ui.base.ActivityAndroidPermissionDelegate;
import org.chromium.ui.base.AndroidPermissionDelegate;

import java.lang.ref.WeakReference;

/**
 * A fragment that provides a UI for scanning caBLE v2 QR codes.
 */
public class CableAuthenticatorUI
        extends Fragment implements OnClickListener, QRScanDialog.Callback {
    private BLEHandler mBleHandler;
    private AndroidPermissionDelegate mPermissionDelegate;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        final Context context = getContext();
        mBleHandler = new BLEHandler();
        if (!mBleHandler.start()) {
            // TODO: handle the case where exporting the GATT server fails.
        }
        mPermissionDelegate = new ActivityAndroidPermissionDelegate(
                new WeakReference<Activity>((Activity) context));
    }

    @Override
    @SuppressLint("SetTextI18n")
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        // Shows a placeholder UI that provides a button for scanning QR codes
        // and a very basic animation for the rest of the screen.

        // TODO: should check FEATURE_BLUETOOTH with
        // https://developer.android.com/reference/android/content/pm/PackageManager.html#hasSystemFeature(java.lang.String)
        final Context context = getContext();
        Button button = new Button(context);
        // TODO: strings should be translated but this will be replaced during
        // the UI process.
        button.setText("Scan QR code");
        button.setOnClickListener(this);

        LinearLayout layout = new LinearLayout(context);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.addView(button);
        // A ProgressBar is included for now. This is in lieu of a future,
        // real UI.
        ProgressBar spinner = new ProgressBar(context);
        spinner.setIndeterminate(true);
        layout.addView(spinner);
        return layout;
    }

    /**
     * Called when the button to scan a QR code is pressed.
     */
    @Override
    public void onClick(View v) {
        // If camera permission is already available, show the scanning
        // dialog.
        final Context context = getContext();
        if (mPermissionDelegate.hasPermission(permission.CAMERA)) {
            (new QRScanDialog(this)).show(getFragmentManager(), "dialog");
            return;
        }

        // Otherwise prompt for permission first.
        if (mPermissionDelegate.canRequestPermission(permission.CAMERA)) {
            // The |Fragment| method |requestPermissions| is called rather than
            // the method on |mPermissionDelegate| because the latter routes the
            // |onRequestPermissionsResult| callback to the Activity, and not
            // this fragment.
            requestPermissions(new String[] {permission.CAMERA}, 1);
        } else {
            // TODO: permission cannot be requested on older versions of
            // Android. Does Chrome always get camera permission at install
            // time on those versions? If so, then this case should be
            // impossible.
        }
    }

    /**
     * Called when the camera has scanned a FIDO QR code.
     */
    @Override
    public void onQRCode(String value) {
        mBleHandler.onQRCode(value);
    }

    /**
     * Called when camera permission has been requested and the user has resolved the permission
     * request.
     */
    @Override
    public void onRequestPermissionsResult(
            int requestCode, String[] permissions, int[] grantResults) {
        mPermissionDelegate = null;

        if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            (new QRScanDialog(this)).show(getFragmentManager(), "dialog");
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mBleHandler.close();
    }
}
