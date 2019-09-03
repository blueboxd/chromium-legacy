// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.password;

import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;

import org.chromium.chrome.R;

/**
 * Password entry editor that allows editing passwords stored in Chrome.
 */
public class PasswordEntryEditor extends Fragment {

    private EditText mSiteText;
    private EditText mUsernameText;
    private EditText mPasswordText;

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        setHasOptionsMenu(true);
        getActivity().setTitle(R.string.password_entry_viewer_edit_stored_password_action_title);
        return inflater.inflate(R.layout.password_entry_editor, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        // Editing action in PasswordEntryViewer sets extras and launches PasswordEntryEditor.
        mSiteText = (EditText) view.findViewById(R.id.site_edit);
        mUsernameText = (EditText) view.findViewById(R.id.username_edit);
        mPasswordText = (EditText) view.findViewById(R.id.password_edit);
        mSiteText.setText(getArguments().getString(SavePasswordsPreferences.PASSWORD_LIST_URL));
        mUsernameText.setText(
                getArguments().getString(SavePasswordsPreferences.PASSWORD_LIST_NAME));
        mPasswordText.setText(
                getArguments().getString(SavePasswordsPreferences.PASSWORD_LIST_PASSWORD));
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        inflater.inflate(R.menu.password_entry_editor_action_bar_menu, menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();
        if (id == R.id.action_save_edited_password) {
            getActivity().finish();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }
}