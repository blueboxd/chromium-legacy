// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler.toolbar;

import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.page_info.ChromePageInfoControllerDelegate;
import org.chromium.chrome.browser.payments.handler.toolbar.PaymentHandlerToolbarMediator.PaymentHandlerToolbarMediatorDelegate;
import org.chromium.components.omnibox.SecurityStatusIcon;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.GURL;

/**
 * PaymentHandlerToolbar coordinator, which owns the component overall, i.e., creates other objects
 * in the component and connects them. It decouples the implementation of this component from other
 * components and acts as the point of contact between them. Any code in this component that needs
 * to interact with another component does that through this coordinator.
 */
public class PaymentHandlerToolbarCoordinator implements PaymentHandlerToolbarMediatorDelegate {
    private PaymentHandlerToolbarView mToolbarView;
    private PaymentHandlerToolbarMediator mMediator;
    private final WebContents mWebContents;
    private final ChromeActivity mActivity;
    private final boolean mIsSmallDevice;

    /**
     * Observer for the error of the payment handler toolbar.
     */
    public interface PaymentHandlerToolbarObserver {
        /** Called when the close button is clicked. */
        void onToolbarCloseButtonClicked();
    }

    /**
     * Constructs the payment-handler toolbar component coordinator.
     * @param context The main activity. Not allowed to be null.
     * @param webContents The {@link WebContents} of the payment handler app. Not allowed to be
     *         null.
     * @param url The url of the payment handler app, i.e., that of
     *         "PaymentRequestEvent.openWindow(url)". Not allowed to be null.
     */
    public PaymentHandlerToolbarCoordinator(
            ChromeActivity context, WebContents webContents, GURL url) {
        assert context != null;
        assert webContents != null;
        assert url != null;
        mWebContents = webContents;
        mActivity = context;
        PropertyModel model = new PropertyModel.Builder(PaymentHandlerToolbarProperties.ALL_KEYS)
                                      .with(PaymentHandlerToolbarProperties.PROGRESS_VISIBLE, true)
                                      .with(PaymentHandlerToolbarProperties.LOAD_PROGRESS,
                                              PaymentHandlerToolbarMediator.MINIMUM_LOAD_PROGRESS)
                                      .with(PaymentHandlerToolbarProperties.SECURITY_ICON,
                                              ConnectionSecurityLevel.NONE)
                                      .with(PaymentHandlerToolbarProperties.URL, url)
                                      .build();
        mIsSmallDevice = !DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);
        mMediator = new PaymentHandlerToolbarMediator(model, webContents, /*delegate=*/this);
        mToolbarView =
                new PaymentHandlerToolbarView(context, /*securityIconOnClickListener=*/mMediator);
        webContents.addObserver(mMediator);
        PropertyModelChangeProcessor.create(
                model, mToolbarView, PaymentHandlerToolbarViewBinder::bind);
    }

    /** Set an observer for PaymentHandlerToolbar. */
    public void setObserver(PaymentHandlerToolbarObserver observer) {
        mToolbarView.setObserver(observer);
    }

    /** @return The height of the toolbar in px. */
    public int getToolbarHeightPx() {
        return mToolbarView.getToolbarHeightPx();
    }

    /** @return The height of the toolbar shadow height in px. */
    public int getShadowHeightPx() {
        return mToolbarView.getShadowHeightPx();
    }

    /** @return The toolbar of the PaymentHandler. */
    public View getView() {
        return mToolbarView.getView();
    }

    /** @return The security icon of the PaymentHandlerToolbar. */
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public void clickSecurityIconForTest() {
        mToolbarView.mSecurityIconView.performClick();
    }

    // Implement PaymentHandlerToolbarMediatorDelegate.
    @Override
    @ConnectionSecurityLevel
    public int getSecurityLevel() {
        return SecurityStateModel.getSecurityLevelForWebContents(mWebContents);
    }

    // Implement PaymentHandlerToolbarMediatorDelegate.
    @Override
    @DrawableRes
    public int getSecurityIconResource(@ConnectionSecurityLevel int securityLevel) {
        return SecurityStatusIcon.getSecurityIconResource(securityLevel,
                SecurityStateModel.shouldShowDangerTriangleForWarningLevel(), mIsSmallDevice,
                /*skipIconForNeutralState=*/true);
    }

    // Implement PaymentHandlerToolbarMediatorDelegate.
    @Override
    public String getSecurityIconContentDescription(@ConnectionSecurityLevel int securityLevel) {
        int contentDescriptionRes =
                SecurityStatusIcon.getSecurityIconContentDescriptionResourceId(securityLevel);
        return mActivity.getResources().getString(contentDescriptionRes);
    }

    // Implement PaymentHandlerToolbarMediatorDelegate.
    @Override
    public void showPageInfoDialog() {
        PageInfoController.show(mActivity, mWebContents, null,
                PageInfoController.OpenedFromSource.TOOLBAR,
                new ChromePageInfoControllerDelegate(mActivity, mWebContents,
                        mActivity::getModalDialogManager,
                        /*offlinePageLoadUrlDelegate=*/
                        new OfflinePageUtils.WebContentsOfflinePageLoadUrlDelegate(mWebContents)));
    }
}
