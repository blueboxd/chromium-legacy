// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.account_picker;

import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.LayoutRes;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.signin.DisplayableProfileData;
import org.chromium.chrome.browser.signin.account_picker.AccountPickerProperties.ExistingAccountRowProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class regroups the buildView and bindView util methods of the
 * existing account row.
 */
class ExistingAccountRowViewBinder {
    private ExistingAccountRowViewBinder() {}

    static View buildView(ViewGroup parent) {
        @LayoutRes
        int layoutRes = ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
                ? R.layout.account_picker_row
                : R.layout.account_picker_row_legacy;
        return LayoutInflater.from(parent.getContext()).inflate(layoutRes, parent, false);
    }

    static void bindView(PropertyModel model, View view, PropertyKey propertyKey) {
        DisplayableProfileData profileData = model.get(ExistingAccountRowProperties.PROFILE_DATA);
        if (propertyKey == ExistingAccountRowProperties.ON_CLICK_LISTENER) {
            view.setOnClickListener(v
                    -> model.get(ExistingAccountRowProperties.ON_CLICK_LISTENER)
                               .onResult(profileData));
        } else if (propertyKey == ExistingAccountRowProperties.PROFILE_DATA) {
            ImageView accountImage = view.findViewById(R.id.account_image);
            accountImage.setImageDrawable(profileData.getImage());

            TextView accountTextPrimary = view.findViewById(R.id.account_text_primary);
            TextView accountTextSecondary = view.findViewById(R.id.account_text_secondary);

            String fullName = profileData.getFullName();
            if (!TextUtils.isEmpty(fullName)) {
                accountTextPrimary.setText(fullName);
                accountTextSecondary.setText(profileData.getAccountName());
                accountTextSecondary.setVisibility(View.VISIBLE);
            } else {
                // Full name is not available, show the email in the primary TextView.
                accountTextPrimary.setText(profileData.getAccountName());
                accountTextSecondary.setVisibility(View.GONE);
            }
        } else if (propertyKey == ExistingAccountRowProperties.IS_SELECTED_ACCOUNT) {
            ImageView selectionMark = view.findViewById(R.id.account_selection_mark);
            selectionMark.setVisibility(model.get(ExistingAccountRowProperties.IS_SELECTED_ACCOUNT)
                            ? View.VISIBLE
                            : View.GONE);
        } else {
            throw new IllegalArgumentException(
                    "Cannot update the view for propertyKey: " + propertyKey);
        }
    }
}
