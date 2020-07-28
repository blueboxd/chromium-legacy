// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.digitalgoods;

import static org.chromium.chrome.browser.browserservices.digitalgoods.DigitalGoodsConverter.convertAcknowledgeCallback;
import static org.chromium.chrome.browser.browserservices.digitalgoods.DigitalGoodsConverter.convertAcknowledgeParams;
import static org.chromium.chrome.browser.browserservices.digitalgoods.DigitalGoodsConverter.convertGetDetailsCallback;
import static org.chromium.chrome.browser.browserservices.digitalgoods.DigitalGoodsConverter.convertGetDetailsParams;
import static org.chromium.chrome.browser.browserservices.digitalgoods.DigitalGoodsConverter.returnClientAppError;
import static org.chromium.chrome.browser.browserservices.digitalgoods.DigitalGoodsConverter.returnClientAppUnavailable;

import android.net.Uri;
import android.os.Bundle;
import android.os.RemoteException;

import org.chromium.chrome.browser.browserservices.TrustedWebActivityClient;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.payments.mojom.DigitalGoods.AcknowledgeResponse;
import org.chromium.payments.mojom.DigitalGoods.GetDetailsResponse;

import androidx.browser.trusted.TrustedWebActivityServiceConnection;

/**
 * This class uses the {@link DigitalGoodsConverter} to convert data types between mojo types and
 * Android types and then uses the {@link TrustedWebActivityClient} to call into the Trusted Web
 * Activity Client.
 */
public class DigitalGoodsAdapter {
    public static final String COMMAND_ACKNOWLEDGE = "acknowledge";
    public static final String COMMAND_GET_DETAILS = "getDetails";
    public static final String KEY_SUCCESS = "success";

    private final TrustedWebActivityClient mClient;

    public DigitalGoodsAdapter(TrustedWebActivityClient client) {
        mClient = client;
    }

    public void getDetails(Uri scope, String[] itemIds, GetDetailsResponse callback) {
        mClient.connectAndExecute(scope, new TrustedWebActivityClient.ExecutionCallback() {
            @Override
            public void onConnected(Origin origin, TrustedWebActivityServiceConnection service)
                    throws RemoteException {
                Bundle result = service.sendExtraCommand(COMMAND_GET_DETAILS,
                        convertGetDetailsParams(itemIds),
                        convertGetDetailsCallback(callback));

                boolean success = result != null &&
                        result.getBoolean(KEY_SUCCESS, false);
                if (!success) {
                    returnClientAppError(callback);
                }
            }

            @Override
            public void onNoTwaFound() {
                returnClientAppUnavailable(callback);
            }
        });
    }

    public void acknowledge(Uri scope, String purchaseToken, boolean makeAvailableAgain,
            AcknowledgeResponse callback) {
        mClient.connectAndExecute(scope, new TrustedWebActivityClient.ExecutionCallback() {
            @Override
            public void onConnected(Origin origin, TrustedWebActivityServiceConnection service)
                    throws RemoteException {
                Bundle result = service.sendExtraCommand(COMMAND_ACKNOWLEDGE,
                        convertAcknowledgeParams(purchaseToken, makeAvailableAgain),
                        convertAcknowledgeCallback(callback));

                boolean success = result != null &&
                        result.getBoolean(KEY_SUCCESS, false);
                if (!success) {
                    returnClientAppError(callback);
                }
            }

            @Override
            public void onNoTwaFound() {
                returnClientAppUnavailable(callback);
            }
        });
    }
}
