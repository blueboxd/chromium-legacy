// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.app.Notification;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import androidx.browser.trusted.Token;
import androidx.browser.trusted.TokenStore;
import androidx.browser.trusted.TrustedWebActivityService;

/**
 * A TrustedWebActivityService to be used in TrustedWebActivityClientTest.
 */
public class TestTrustedWebActivityService extends TrustedWebActivityService {
    // TODO(peconn): Add an image resource to chrome_public_test_support, supply that in
    // getSmallIconId and verify it is used in notifyNotificationWithChannel.
    public static final int SMALL_ICON_ID = 42;

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
