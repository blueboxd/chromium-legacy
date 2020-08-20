// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.digitalgoods;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.payments.mojom.DigitalGoods;
import org.chromium.services.service_manager.InterfaceFactory;

/**
 * A factory to create instances of the {@link DigitalGoods} mojo interface.
 */
public class DigitalGoodsFactory implements InterfaceFactory<DigitalGoods> {
    private static DigitalGoods sImplForTesting;

    private final RenderFrameHost mRenderFrameHost;
    private final DigitalGoodsImpl.Delegate mDigitalGoodsDelegate;
    private final DigitalGoodsAdapter mAdapter;

    @VisibleForTesting
    public static void setDigitalGoodsForTesting(DigitalGoods impl) {
        sImplForTesting = impl;
    }

    public DigitalGoodsFactory(RenderFrameHost renderFrameHost) {
        mRenderFrameHost = renderFrameHost;
        mDigitalGoodsDelegate = mRenderFrameHost::getLastCommittedURL;
        mAdapter = new DigitalGoodsAdapter(
                ChromeApplication.getComponent().resolveTrustedWebActivityClient());
    }

    @Override
    public DigitalGoods createImpl() {
        if (sImplForTesting != null) return sImplForTesting;
        if (!PaymentFeatureList.isEnabled(PaymentFeatureList.WEB_PAYMENTS_APP_STORE_BILLING)) {
            return null;
        }

        // Ensure that the DigitalGoodsImpl is only created if we're in a TWA and on its verified
        // origin.
        WebContents wc = WebContentsStatics.fromRenderFrameHost(mRenderFrameHost);
        ChromeActivity<?> activity = ChromeActivity.fromWebContents(wc);
        if (!(activity instanceof CustomTabActivity)) return null;
        CustomTabActivity cta = (CustomTabActivity) activity;
        if (!cta.isInTwaMode()) return null;

        // TODO(peconn): Add a test for this.

        return new DigitalGoodsImpl(mAdapter, mDigitalGoodsDelegate);
    }
}
