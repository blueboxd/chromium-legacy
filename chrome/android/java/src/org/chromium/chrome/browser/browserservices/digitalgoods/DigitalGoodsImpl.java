// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.digitalgoods;

import android.net.Uri;

import org.chromium.mojo.system.MojoException;
import org.chromium.payments.mojom.DigitalGoods;
import org.chromium.payments.mojom.DigitalGoods.AcknowledgeResponse;
import org.chromium.payments.mojom.DigitalGoods.GetDetailsResponse;

/**
 * An implementation of the {@link DigitalGoods} mojo interface that communicates with Trusted Web
 * Activity clients to call Billing APIs.
 */
public class DigitalGoodsImpl implements DigitalGoods {
    private final DigitalGoodsAdapter mAdapter;
    private final Delegate mDelegate;

    /** A Delegate that provides the current URL. */
    public interface Delegate {
        String getUrl();
    }

    /** Constructs the object with a given adapter and delegate. */
    public DigitalGoodsImpl(DigitalGoodsAdapter adapter,
            Delegate delegate) {
        mAdapter = adapter;
        mDelegate = delegate;
    }

    @Override
    public void getDetails(String[] itemIds, GetDetailsResponse callback) {
        mAdapter.getDetails(Uri.parse(mDelegate.getUrl()), itemIds, callback);
    }

    @Override
    public void acknowledge(String purchaseToken, boolean makeAvailableAgain,
            AcknowledgeResponse callback) {
        mAdapter.acknowledge(
                Uri.parse(mDelegate.getUrl()), purchaseToken, makeAvailableAgain,callback);
    }

    @Override
    public void close() {}

    @Override
    public void onConnectionError(MojoException e) {}
}
