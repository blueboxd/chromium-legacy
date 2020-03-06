// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler.toolbar;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.payments.handler.toolbar.PaymentHandlerToolbarCoordinator.PaymentHandlerToolbarObserver;
import org.chromium.components.browser_ui.widget.FadingShadow;
import org.chromium.components.browser_ui.widget.FadingShadowView;

/** PaymentHandlerToolbar UI. */
/* package */ class PaymentHandlerToolbarView {
    private final int mToolbarHeightPx;
    private final int mShadowHeightPx;

    private final View mToolbarView;
    /* package */ final TextView mOriginView;
    /* package */ final TextView mTitleView;
    /* package */ final ProgressBar mProgressBar;
    /* package */ final ImageView mSecurityIconView;

    private PaymentHandlerToolbarObserver mObserver;

    /**
     * Construct the PaymentHandlerToolbarView.
     *
     * @param context The context where the bottome-sheet should be shown.
     * @param securityIconOnClickListener The listener to clicking the security icon.
     */
    /* package */ PaymentHandlerToolbarView(
            Context context, OnClickListener securityIconOnClickListener) {
        mToolbarHeightPx =
                context.getResources().getDimensionPixelSize(R.dimen.sheet_tab_toolbar_height);
        mShadowHeightPx =
                context.getResources().getDimensionPixelSize(R.dimen.action_bar_shadow_height);

        mToolbarView = LayoutInflater.from(context).inflate(R.layout.sheet_tab_toolbar, null);

        mOriginView = mToolbarView.findViewById(R.id.origin);
        mTitleView = mToolbarView.findViewById(R.id.title);
        mProgressBar = mToolbarView.findViewById(R.id.progress_bar);
        mSecurityIconView = mToolbarView.findViewById(R.id.security_icon);
        View closeButton = mToolbarView.findViewById(R.id.close);
        closeButton.setOnClickListener(view -> {
            assert mObserver != null;
            mObserver.onToolbarCloseButtonClicked();
        });
        mSecurityIconView.setOnClickListener(securityIconOnClickListener);

        // These parts from sheet_tab_toolbar are not needed in this component.
        mToolbarView.findViewById(R.id.open_in_new_tab).setVisibility(View.GONE);
        mToolbarView.findViewById(R.id.favicon).setVisibility(View.GONE);

        FadingShadowView shadow = mToolbarView.findViewById(R.id.shadow);
        shadow.init(ApiCompatibilityUtils.getColor(
                            context.getResources(), R.color.toolbar_shadow_color),
                FadingShadow.POSITION_TOP);
    }

    /** Set an observer for this class. */
    /* package */ void setObserver(PaymentHandlerToolbarObserver observer) {
        mObserver = observer;
    }

    /** @return The height of the toolbar in px. */
    /* package */ int getToolbarHeightPx() {
        return mToolbarHeightPx;
    }

    /** @return The height of the toolbar shadow height in px. */
    /* packaged */ int getShadowHeightPx() {
        return mShadowHeightPx;
    }

    /** @return The layout of the PaymentHandlerToolbar. */
    /* package */ View getView() {
        return mToolbarView;
    }
}
