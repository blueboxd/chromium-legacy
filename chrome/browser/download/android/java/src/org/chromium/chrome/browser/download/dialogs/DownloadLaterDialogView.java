// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.LinearLayout;
import android.widget.RadioGroup;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.download.R;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The custom view in the download later dialog.
 */
public class DownloadLaterDialogView
        extends LinearLayout implements RadioGroup.OnCheckedChangeListener {
    private RadioButtonWithDescription mDownloadNow;
    private RadioButtonWithDescription mOnWifi;
    private RadioButtonWithDescription mDownloadLater;
    private RadioGroup mRadioGroup;

    // The item that the user selected in the download later dialog UI.
    private @DownloadLaterDialogChoice int mChoice = DownloadLaterDialogChoice.DOWNLOAD_NOW;

    /**
     * The view binder to propagate events from model to view.
     */
    public static class Binder {
        public static void bind(
                PropertyModel model, DownloadLaterDialogView view, PropertyKey propertyKey) {
            if (propertyKey == DownloadLaterDialogProperties.DOWNLOAD_TIME_INITIAL_SELECTION) {
                view.setChoice(
                        model.get(DownloadLaterDialogProperties.DOWNLOAD_TIME_INITIAL_SELECTION));
            }
        }
    }

    public DownloadLaterDialogView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Initialize the internal objects.
     */
    void initialize() {
        mDownloadNow = (RadioButtonWithDescription) findViewById(R.id.download_now);
        mOnWifi = (RadioButtonWithDescription) findViewById(R.id.on_wifi);
        mDownloadLater = (RadioButtonWithDescription) findViewById(R.id.choose_date_time);
        mRadioGroup = (RadioGroup) findViewById(R.id.radio_button_layout);
        mRadioGroup.setOnCheckedChangeListener(this);
    }

    void setChoice(@DownloadLaterDialogChoice int choice) {
        switch (choice) {
            case DownloadLaterDialogChoice.DOWNLOAD_NOW:
                mDownloadNow.setChecked(true);
                break;
            case DownloadLaterDialogChoice.ON_WIFI:
                mOnWifi.setChecked(true);
                break;
            case DownloadLaterDialogChoice.DOWNLOAD_LATER:
                mDownloadLater.setChecked(true);
                break;
        }

        mChoice = choice;
    }

    public @DownloadLaterDialogChoice int getChoice() {
        return mChoice;
    }

    // RadioGroup.OnCheckedChangeListener overrides.
    @Override
    public void onCheckedChanged(RadioGroup radioGroup, int index) {
        @DownloadLaterDialogChoice
        int choice = DownloadLaterDialogChoice.DOWNLOAD_NOW;
        if (mDownloadNow.isChecked()) {
            choice = DownloadLaterDialogChoice.DOWNLOAD_NOW;
        } else if (mOnWifi.isChecked()) {
            choice = DownloadLaterDialogChoice.ON_WIFI;
        } else if (mDownloadLater.isChecked()) {
            choice = DownloadLaterDialogChoice.DOWNLOAD_LATER;
        }
        mChoice = choice;
    }
}
