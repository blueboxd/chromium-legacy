// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.page_info;

import android.content.Context;
import android.support.v7.widget.SwitchCompat;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.widget.CompoundButton;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.settings.website.CookieControlsControllerStatus;

/**
 * View showing a toggle and a description for third-party cookie blocking for a site.
 */
public class CookieControlsView
        extends LinearLayout implements CompoundButton.OnCheckedChangeListener {
    private final SwitchCompat mSwitch;
    private final TextView mBlockedText;
    private boolean mLastSwitchState;
    private CookieControlsParams mParams;

    /**  Parameters to configure the cookie controls view. */
    public static class CookieControlsParams {
        // Called when the cookie controls UI is closed.
        public Runnable onUiClosingCallback;
        // Called when the toggle controlling third-party cookie blocking changes.
        public Callback<Boolean> onCheckedChangedCallback;
    }

    public CookieControlsView(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        setOrientation(VERTICAL);
        setLayoutParams(new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        LayoutInflater.from(context).inflate(R.layout.cookie_controls_view, this, true);
        mBlockedText = findViewById(R.id.cookie_controls_blocked_cookies_text);
        mSwitch = findViewById(R.id.cookie_controls_block_cookies_switch);
        mSwitch.setOnCheckedChangeListener(this);
    }

    void setParams(CookieControlsParams params) {
        mParams = params;
    }

    // FrameLayout:
    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mParams.onUiClosingCallback.run();
    }

    public void setCookieBlockingStatus(@CookieControlsControllerStatus int status) {
        mLastSwitchState = status == CookieControlsControllerStatus.ENABLED;
        mSwitch.setChecked(mLastSwitchState);
        mSwitch.setEnabled(status != CookieControlsControllerStatus.DISABLED);
    }

    public void setBlockedCookiesCount(int blockedCookies) {
        mBlockedText.setText(String.format(
                getContext().getString(R.string.cookie_controls_blocked_cookies), blockedCookies));
    }

    // CompoundButton.OnCheckedChangeListener:
    @Override
    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
        if (isChecked == mLastSwitchState) return;
        assert buttonView == mSwitch;
        mParams.onCheckedChangedCallback.onResult(isChecked);
    }
}
