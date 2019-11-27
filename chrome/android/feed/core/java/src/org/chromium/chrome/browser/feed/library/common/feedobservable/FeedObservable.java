// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.feedobservable;

import org.chromium.chrome.browser.feed.library.common.logging.Logger;

import java.util.HashSet;
import java.util.Set;

/** Provides methods for registering or unregistering arbitrary observers. */
public abstract class FeedObservable<ObserverT> implements Observable<ObserverT> {
    private static final String TAG = "FeedObservable";

    protected final Set<ObserverT> mObservers = new HashSet<>();

    /** Adds given {@code observer}. No-op if the observer has already been added. */
    @Override
    public void registerObserver(ObserverT observer) {
        synchronized (mObservers) {
            if (!mObservers.add(observer)) {
                Logger.w(TAG, "Registering observer: %s multiple times.", observer);
            }
        }
    }

    /** Removes given {@code observer}. No-op if the observer is not currently added. */
    @Override
    public void unregisterObserver(ObserverT observer) {
        synchronized (mObservers) {
            if (!mObservers.remove(observer)) {
                Logger.w(TAG, "Unregistering observer: %s that isn't registered.", observer);
            }
        }
    }
}
