// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler.toolbar;

import android.view.View;
import android.view.View.OnClickListener;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.page_info.PageInfoController;
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
    private final WebContents mWebContents;

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
    public PaymentHandlerToolbarCoordinator(ChromeActivity context, WebContents webContents,
            URI url, PaymentHandlerToolbarObserver observer) {
        mWebContents = webContents;
        OnClickListener securityIconOnClickListener = v -> {
            if (context == null) return;
            PageInfoController.show(context, webContents, null,
                    PageInfoController.OpenedFromSource.TOOLBAR,
                    /*offlinePageLoadUrlDelegate=*/
                    new OfflinePageUtils.WebContentsOfflinePageLoadUrlDelegate(webContents));
        };
        mToolbarView =
                new PaymentHandlerToolbarView(context, securityIconOnClickListener, observer);
        PropertyModel model = new PropertyModel.Builder(PaymentHandlerToolbarProperties.ALL_KEYS)
                                      .with(PaymentHandlerToolbarProperties.PROGRESS_VISIBLE, true)
                                      .with(PaymentHandlerToolbarProperties.LOAD_PROGRESS,
                                              PaymentHandlerToolbarMediator.MINIMUM_LOAD_PROGRESS)
                                      .with(PaymentHandlerToolbarProperties.SECURITY_ICON,
                                              ConnectionSecurityLevel.NONE)
                                      .with(PaymentHandlerToolbarProperties.URL, url)
                                      .build();
        PaymentHandlerToolbarMediator mediator =
                new PaymentHandlerToolbarMediator(model, webContents, observer);
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
