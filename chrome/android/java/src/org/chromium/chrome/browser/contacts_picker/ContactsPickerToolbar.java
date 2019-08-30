// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contacts_picker;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.Button;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.selection.SelectableListToolbar;

import java.util.List;

/**
 * Handles toolbar functionality for the {@ContactsPickerDialog}.
 */
public class ContactsPickerToolbar extends SelectableListToolbar<ContactDetails> {
    // Our parent dialog.
    ContactsPickerDialog mDialog;

    public ContactsPickerToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Set the parent dialog for this toolbar.
     */
    public void setParentDialog(ContactsPickerDialog dialog) {
        mDialog = dialog;
    }

    /**
     * Shows the Back arrow navigation button in the upper left corner.
     */
    public void showBackArrow() {
        setNavigationButton(NAVIGATION_BUTTON_BACK);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        TextView up = (TextView) mNumberRollView.findViewById(R.id.up);
        TextView down = (TextView) mNumberRollView.findViewById(R.id.down);
        ApiCompatibilityUtils.setTextAppearance(up, R.style.TextAppearance_BlackHeadline);
        ApiCompatibilityUtils.setTextAppearance(down, R.style.TextAppearance_BlackHeadline);
    }

    @Override
    public void onNavigationBack() {
        if (isSearching()) {
            super.onNavigationBack();
        } else {
            mDialog.cancel();
        }
    }

    @Override
    protected void showSelectionView(
            List<ContactDetails> selectedItems, boolean wasSelectionEnabled) {
        switchToNumberRollView(selectedItems, wasSelectionEnabled);
    }

    @Override
    public void onSelectionStateChange(List<ContactDetails> selectedItems) {
        super.onSelectionStateChange(selectedItems);

        Button done = (Button) findViewById(R.id.done);
        done.setEnabled(selectedItems.size() > 0);

        showBackArrow();
    }
}
