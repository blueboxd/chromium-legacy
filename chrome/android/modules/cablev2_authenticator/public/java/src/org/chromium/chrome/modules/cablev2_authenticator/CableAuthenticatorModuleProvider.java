// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.cablev2_authenticator;

import android.annotation.SuppressLint;
import android.content.Context;
import android.os.Bundle;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentTransaction;

/**
 * Provides a UI that attempts to install the caBLEv2 Authenticator module. If already installed, or
 * successfully installed, it replaces itself in the back-stack with the authenticator UI.
 *
 * This code lives in the base module, i.e. is _not_ part of the dynamically-loaded module.
 *
 * This does not use {@link ModuleInstallUi} because it needs to integrate into the Fragment-based
 * settings UI, while {@link ModuleInstallUi} assumes that the UI does in a {@link Tab}.
 */
public class CableAuthenticatorModuleProvider extends Fragment {
    private TextView mStatus;

    @Override
    @SuppressLint("SetTextI18n")
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        final Context context = getContext();

        // This UI is a placeholder for development, has not been reviewed by
        // UX, and thus just uses untranslated strings for now.
        getActivity().setTitle("Installing");

        mStatus = new TextView(context);
        mStatus.setPadding(0, 60, 0, 60);

        LinearLayout layout = new LinearLayout(context);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setGravity(Gravity.CENTER_HORIZONTAL);
        layout.addView(mStatus,
                new LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT,
                        LinearLayout.LayoutParams.WRAP_CONTENT));

        if (Cablev2AuthenticatorModule.isInstalled()) {
            showModule();
        } else {
            mStatus.setText("Installing security key functionality…");
            Cablev2AuthenticatorModule.install((success) -> {
                if (!success) {
                    mStatus.setText("Failed to install.");
                    return;
                }
                showModule();
            });
        }

        return layout;
    }

    @SuppressLint("SetTextI18n")
    private void showModule() {
        mStatus.setText("Installed.");

        FragmentTransaction transaction =
                getActivity().getSupportFragmentManager().beginTransaction();
        transaction.replace(getId(), Cablev2AuthenticatorModule.getImpl().getFragment());
        // This fragment is deliberately not added to the back-stack here so
        // that it appears to have been "replaced" by the authenticator UI.
        transaction.commit();
    }
}
