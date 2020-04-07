// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler;

import android.content.Context;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.payments.PaymentsExperimentalFeatures;
import org.chromium.chrome.browser.payments.handler.toolbar.PaymentHandlerToolbarCoordinator;
import org.chromium.chrome.browser.thinwebview.ThinWebView;
import org.chromium.chrome.browser.thinwebview.ThinWebViewConstraints;
import org.chromium.chrome.browser.thinwebview.ThinWebViewFactory;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.GURL;

/**
 * PaymentHandler coordinator, which owns the component overall, i.e., creates other objects in the
 * component and connects them. It decouples the implementation of this component from other
 * components and acts as the point of contact between them. Any code in this component that needs
 * to interact with another component does that through this coordinator.
 */
public class PaymentHandlerCoordinator {
    private Runnable mHider;
    private WebContents mWebContents;
    private PaymentHandlerToolbarCoordinator mToolbarCoordinator;

    /** Constructs the payment-handler component coordinator. */
    public PaymentHandlerCoordinator() {
        assert isEnabled();
    }

    /** Observes the state changes of the payment-handler UI. */
    public interface PaymentHandlerUiObserver {
        /** Called when Payment Handler UI is closed. */
        void onPaymentHandlerUiClosed();
        /** Called when Payment Handler UI is shown. */
        void onPaymentHandlerUiShown();
    }

    /** Observes the WebContents of the payment-handler UI. */
    public interface PaymentHandlerWebContentsObserver {
        /**
         * Called when the WebContents has been initialized.
         * @param webContents The WebContents of the PaymentHandler.
         */
        void onWebContentsInitialized(WebContents webContents);
    }

    /**
     * Shows the payment-handler UI.
     *
     * @param chromeActivity The activity where the UI should be shown.
     * @param url The url of the payment handler app, i.e., that of
     *         "PaymentRequestEvent.openWindow(url)".
     * @param isIncognito Whether the tab is in incognito mode.
     * @param webContentsObserver The observer of the WebContents of the
     *         PaymentHandler.
     * @param uiObserver The {@link PaymentHandlerUiObserver} that observes this Payment Handler UI.
     * @return Whether the payment-handler UI was shown. Can be false if the UI was suppressed.
     */
    public boolean show(ChromeActivity activity, GURL url, boolean isIncognito,
            PaymentHandlerWebContentsObserver webContentsObserver,
            PaymentHandlerUiObserver uiObserver) {
        assert mHider == null : "Already showing payment-handler UI";

        mWebContents = WebContentsFactory.createWebContents(isIncognito, /*initiallyHidden=*/false);
        ContentView webContentView = ContentView.createContentView(activity, mWebContents);
        mWebContents.initialize(ChromeVersionInfo.getProductVersion(),
                ViewAndroidDelegate.createBasicDelegate(webContentView), webContentView,
                activity.getWindowAndroid(), WebContents.createDefaultInternalsHolder());
        webContentsObserver.onWebContentsInitialized(mWebContents);
        mWebContents.getNavigationController().loadUrl(new LoadUrlParams(url.getSpec()));

        mToolbarCoordinator = new PaymentHandlerToolbarCoordinator(activity, mWebContents, url);

        PropertyModel model = new PropertyModel.Builder(PaymentHandlerProperties.ALL_KEYS).build();
        PaymentHandlerMediator mediator = new PaymentHandlerMediator(model, this::hide,
                mWebContents, uiObserver, activity.getActivityTab().getView(),
                mToolbarCoordinator.getToolbarHeightPx(),
                calculateBottomSheetToolbarContainerTopPadding(activity));
        activity.getWindow().getDecorView().addOnLayoutChangeListener(mediator);
        BottomSheetController bottomSheetController = activity.getBottomSheetController();
        bottomSheetController.addObserver(mediator);
        mWebContents.addObserver(mediator);

        // Observer is designed to set here rather than in the constructor because
        // PaymentHandlerMediator and PaymentHandlerToolbarCoordinator have mutual dependencies.
        mToolbarCoordinator.setObserver(mediator);
        ThinWebView thinWebView = ThinWebViewFactory.create(
                activity, new ActivityWindowAndroid(activity), new ThinWebViewConstraints());
        assert webContentView.getParent() == null;
        thinWebView.attachWebContents(mWebContents, webContentView, null);
        PaymentHandlerView view = new PaymentHandlerView(
                activity, mWebContents, mToolbarCoordinator.getView(), thinWebView.getView());
        assert mToolbarCoordinator.getToolbarHeightPx() == view.getToolbarHeightPx();
        PropertyModelChangeProcessor changeProcessor =
                PropertyModelChangeProcessor.create(model, view, PaymentHandlerViewBinder::bind);
        mHider = () -> {
            changeProcessor.destroy();
            bottomSheetController.removeObserver(mediator);
            bottomSheetController.hideContent(/*content=*/view, /*animate=*/true);
            mWebContents.destroy();
            uiObserver.onPaymentHandlerUiClosed();
            assert activity.getWindow() != null;
            assert activity.getWindow().getDecorView() != null;
            activity.getWindow().getDecorView().removeOnLayoutChangeListener(mediator);
            thinWebView.destroy();
            mediator.destroy();
        };
        return bottomSheetController.requestShowContent(view, /*animate=*/true);
    }

    // TODO(crbug.com/1059269): This approach introduces coupling by assuming BottomSheet toolbar's
    // layout. Remove it once we can calculate visible content height with better ways.
    /**
     * @return bottom_sheet_toolbar_container's top padding. This is used to calculate the visible
     *         content height.
     */
    private int calculateBottomSheetToolbarContainerTopPadding(Context context) {
        View view = new View(context);
        view.setBackgroundResource(R.drawable.top_round);
        return view.getPaddingTop();
    }

    /**
     * Get the WebContents of the Payment Handler for testing purpose. In other situations,
     * WebContents should not be leaked outside the Payment Handler.
     *
     * @return The WebContents of the Payment Handler.
     */
    @VisibleForTesting
    public WebContents getWebContentsForTest() {
        return mWebContents;
    }

    /** Hides the payment-handler UI. */
    public void hide() {
        if (mHider == null) return;
        mHider.run();
        mHider = null;
    }

    /**
     * @return Whether this solution (as opposed to the Chrome-custom-tab based solution) of
     *     PaymentHandler is enabled. This solution is intended to replace the other
     *     solution.
     */
    public static boolean isEnabled() {
        // Enabling the flag of either ScrollToExpand or PaymentsExperimentalFeatures will enable
        // this feature.
        return PaymentsExperimentalFeatures.isEnabled(
                ChromeFeatureList.SCROLL_TO_EXPAND_PAYMENT_HANDLER);
    }

    @VisibleForTesting
    public void clickSecurityIconForTest() {
        mToolbarCoordinator.clickSecurityIconForTest();
    }
}
