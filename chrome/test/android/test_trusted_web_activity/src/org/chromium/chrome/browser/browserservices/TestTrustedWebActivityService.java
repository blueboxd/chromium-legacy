// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.app.Notification;
import android.os.Bundle;
import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.browser.trusted.Token;
import androidx.browser.trusted.TokenStore;
import androidx.browser.trusted.TrustedWebActivityCallbackRemote;
import androidx.browser.trusted.TrustedWebActivityService;

/**
 * A TrustedWebActivityService to be used in TrustedWebActivityClientTest.
 */
public class TestTrustedWebActivityService extends TrustedWebActivityService {
    // TODO(peconn): Add an image resource to chrome_public_test_support, supply that in
    // getSmallIconId and verify it is used in notifyNotificationWithChannel.
    public static final int SMALL_ICON_ID = 42;

    private static final String CHECK_LOCATION_PERMISSION_COMMAND_NAME =
            "checkAndroidLocationPermission";
    private static final String LOCATION_PERMISSION_RESULT = "locationPermissionResult";
    private static final String START_LOCATION_COMMAND_NAME = "startLocation";
    private static final String STOP_LOCATION_COMMAND_NAME = "stopLocation";
    private static final String LOCATION_ARG_ENABLE_HIGH_ACCURACY = "enableHighAccuracy";
    private static final String EXTRA_NEW_LOCATION_AVAILABLE_CALLBACK = "onNewLocationAvailable";
    private static final String EXTRA_NEW_LOCATION_ERROR_CALLBACK = "onNewLocationError";
    private static final String EXTRA_COMMAND_SUCCESS = "success";

    private final TokenStore mTokenStore = new InMemoryStore();

    @Override
    public void onCreate() {
        super.onCreate();

        Token chromeTestToken = Token.create("org.chromium.chrome.tests", getPackageManager());
        mTokenStore.store(chromeTestToken);
    }

    @NonNull
    @Override
    public TokenStore getTokenStore() {
        return mTokenStore;
    }

    @Override
    public boolean onNotifyNotificationWithChannel(String platformTag, int platformId,
            Notification notification, String channelName) {
        MessengerService.sMessageHandler
                .recordNotifyNotification(platformTag, platformId, channelName);
        return true;
    }

    @Override
    public void onCancelNotification(String platformTag, int platformId) {
        MessengerService.sMessageHandler.recordCancelNotification(platformTag, platformId);
    }

    @Override
    public int onGetSmallIconId() {
        MessengerService.sMessageHandler.recordGetSmallIconId();
        return SMALL_ICON_ID;
    }

    @Override
    public Bundle onExtraCommand(
            String commandName, Bundle args, @Nullable TrustedWebActivityCallbackRemote callback) {
        Bundle executionResult = new Bundle();
        switch (commandName) {
            case CHECK_LOCATION_PERMISSION_COMMAND_NAME: {
                if (callback == null) break;

                Bundle permission = new Bundle();
                permission.putBoolean(LOCATION_PERMISSION_RESULT, true);
                try {
                    callback.runExtraCallback(CHECK_LOCATION_PERMISSION_COMMAND_NAME, permission);
                } catch (RemoteException e) {
                }

                executionResult.putBoolean(EXTRA_COMMAND_SUCCESS, true);
                break;
            }
            case START_LOCATION_COMMAND_NAME: {
                if (callback == null) break;

                Bundle locationResult = new Bundle();
                locationResult.putDouble("latitude", 1.0);
                locationResult.putDouble("longitude", -2.0);
                locationResult.putDouble("accuracy", 0.5);
                locationResult.putLong("timeStamp", System.currentTimeMillis());
                try {
                    callback.runExtraCallback(
                            EXTRA_NEW_LOCATION_AVAILABLE_CALLBACK, locationResult);
                } catch (RemoteException e) {
                }

                executionResult.putBoolean(EXTRA_COMMAND_SUCCESS, true);
                break;
            }
            case STOP_LOCATION_COMMAND_NAME: {
                executionResult.putBoolean(EXTRA_COMMAND_SUCCESS, true);
                break;
            }
            default:
                executionResult.putBoolean(EXTRA_COMMAND_SUCCESS, false);
        }
        return executionResult;
    }

    private static class InMemoryStore implements TokenStore {
        private Token mToken;

        @Override
        public void store(@Nullable Token token) {
            mToken = token;
        }

        @Nullable
        @Override
        public Token load() {
            return mToken;
        }
    }
}
