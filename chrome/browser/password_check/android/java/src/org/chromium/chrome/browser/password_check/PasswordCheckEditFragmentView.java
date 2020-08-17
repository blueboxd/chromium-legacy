// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import android.os.Bundle;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.preference.PreferenceFragmentCompat;

/**
 * This class is responsible for rendering the edit fragment where users can provide a new password
 * for compromised credentials.
 * TODO(crbug.com/1092444): Make this a component that is reusable in password settings as well.
 */
public class PasswordCheckEditFragmentView extends PreferenceFragmentCompat {
    public static final String EXTRA_COMPROMISED_CREDENTIAL = "extra_compromised_credential";
    @VisibleForTesting
    static final String EXTRA_NEW_PASSWORD = "extra_new_password";

    private String mNewPassword;
    private CompromisedCredential mCredential;

    private EditText mPasswordText;

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {}

    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        setHasOptionsMenu(true);
        getActivity().setTitle(R.string.password_entry_viewer_edit_stored_password_action_title);
        return inflater.inflate(R.layout.password_check_edit_fragment, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        mCredential = getCredentialFromInstanceStateOrLaunchBundle(savedInstanceState);
        mNewPassword = getNewPasswordFromInstanceStateOrLaunchBundle(savedInstanceState);

        EditText siteText = (EditText) view.findViewById(R.id.site_edit);
        siteText.setText(mCredential.getDisplayOrigin());

        EditText usernameText = (EditText) view.findViewById(R.id.username_edit);
        usernameText.setText(mCredential.getDisplayUsername());

        mPasswordText = (EditText) view.findViewById(R.id.password_edit);
        mPasswordText.setText(mCredential.getPassword());
        mPasswordText.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {}

            @Override
            public void afterTextChanged(Editable editable) {
                mNewPassword = mPasswordText.getText().toString();
                if (TextUtils.isEmpty(mNewPassword)) {
                    // TODO(crbug.com/1114720): setError on R.id.password_label.
                }
            }
        });
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear(); // Remove help and feedback for this screen.
        inflater.inflate(R.menu.password_check_editor_action_bar_menu, menu);
        // TODO(crbug.com/1092444): Make the back arrow an 'X'.
    }

    @Override
    public void onSaveInstanceState(@NonNull Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putParcelable(EXTRA_COMPROMISED_CREDENTIAL, mCredential);
        outState.putString(EXTRA_NEW_PASSWORD, mNewPassword);
    }

    private CompromisedCredential getCredentialFromInstanceStateOrLaunchBundle(
            Bundle savedInstanceState) {
        if (savedInstanceState != null
                && savedInstanceState.containsKey(EXTRA_COMPROMISED_CREDENTIAL)) {
            return savedInstanceState.getParcelable(EXTRA_COMPROMISED_CREDENTIAL);
        }
        Bundle extras = getArguments();
        assert extras != null
                && extras.containsKey(EXTRA_COMPROMISED_CREDENTIAL)
            : "PasswordCheckEditFragmentView must be launched with a compromised credential "
                        + "extra!";
        return extras.getParcelable(EXTRA_COMPROMISED_CREDENTIAL);
    }

    private String getNewPasswordFromInstanceStateOrLaunchBundle(Bundle savedInstanceState) {
        if (savedInstanceState != null && savedInstanceState.containsKey(EXTRA_NEW_PASSWORD)) {
            return savedInstanceState.getParcelable(EXTRA_NEW_PASSWORD);
        }
        Bundle extras = getArguments();
        if (extras != null && extras.containsKey(EXTRA_NEW_PASSWORD)) {
            return extras.getParcelable(EXTRA_NEW_PASSWORD);
        }
        return mCredential.getPassword();
    }
}
