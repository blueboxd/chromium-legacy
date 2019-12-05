// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler.toolbar;

import android.content.Context;
import android.view.View;

import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.net.URI;

/**
 * PaymentHandlerToolbar coordinator, which owns the component overall, i.e., creates other objects
 * in the component and connects them. It decouples the implementation of this component from other
 * components and acts as the point of contact between them. Any code in this component that needs
 * to interact with another component does that through this coordinator.
 */
public class PaymentHandlerToolbarCoordinator {
    private Runnable mHider;
    private PaymentHandlerToolbarView mToolbarView;

    /**
     * Observer for the error of the payment handler toolbar.
     */
    public interface PaymentHandlerToolbarObserver {
        /** Called when the UI gets an error. */
        void onToolbarError();

        /** Called when the close button is clicked. */
        void onToolbarCloseButtonClicked();
    }

    /**
     * Constructs the payment-handler toolbar component coordinator.
     * @param context The main activity.
     * @param webContents The {@link WebContents} of the payment handler app.
     * @param url The url of the payment handler app, i.e., that of
     *         "PaymentRequestEvent.openWindow(url)".
     * @param observer The observer of this toolbar.
     */
    public PaymentHandlerToolbarCoordinator(Context context, WebContents webContents, URI url,
            PaymentHandlerToolbarObserver observer) {
        mToolbarView = new PaymentHandlerToolbarView(context, observer);
        PropertyModel model = new PropertyModel.Builder(PaymentHandlerToolbarProperties.ALL_KEYS)
                                      .with(PaymentHandlerToolbarProperties.PROGRESS_VISIBLE, true)
                                      .with(PaymentHandlerToolbarProperties.LOAD_PROGRESS, 0)
                                      .with(PaymentHandlerToolbarProperties.SECURITY_ICON,
                                              ConnectionSecurityLevel.NONE)
                                      .build();
        PaymentHandlerToolbarMediator mediator =
                new PaymentHandlerToolbarMediator(model, webContents, url, observer);
        webContents.addObserver(mediator);
        PropertyModelChangeProcessor changeProcessor = PropertyModelChangeProcessor.create(
                model, mToolbarView, PaymentHandlerToolbarViewBinder::bind);
    }

    /** @return The height of the toolbar in px. */
    public int getToolbarHeightPx() {
        return mToolbarView.getToolbarHeightPx();
    }

    /** @return The toolbar of the PaymentHandler. */
    public View getView() {
        return mToolbarView.getView();
    }
}
