// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;

import org.chromium.android_webview.common.services.IVariationsSeedServer;

import javax.annotation.concurrent.GuardedBy;

/**
 * VariationsSeedServer is a bound service that shares the Variations seed with all the WebViews
 * on the system. A WebView will bind and call getSeed, passing a file descriptor to which the
 * service should write the seed. The service will convert itself into a started service while
 * processing seed requests, and stop itself when the last outstanding request has completed.
 */
public class VariationsSeedServer extends Service {
    private final IVariationsSeedServer.Stub mBinder = new IVariationsSeedServer.Stub() {
        // Stores the number of pending getSeed calls. When all calls have completed, we can stop
        // the service.
        // Usage of this variable must be synchronized as getSeed calls can be dispatched to
        // multiple threads simultaneously.
        @GuardedBy("mLock")
        private int mPendingGetSeedCalls = 0;
        private final Object mLock = new Object();

        @Override
        public void getSeed(ParcelFileDescriptor newSeedFile, long oldSeedDate) {
            synchronized (mLock) {
                if (mPendingGetSeedCalls++ == 0) {
                    startService(new Intent(VariationsSeedServer.this, VariationsSeedServer.class));
                }
            }
            VariationsSeedHolder.getInstance().writeSeedIfNewer(
                    newSeedFile, oldSeedDate, /*onFinished=*/this::onGetSeedComplete);
        }

        private void onGetSeedComplete() {
            synchronized (mLock) {
                if (--mPendingGetSeedCalls == 0) {
                    stopSelf();
                }
            }
        }
    };

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }
}
