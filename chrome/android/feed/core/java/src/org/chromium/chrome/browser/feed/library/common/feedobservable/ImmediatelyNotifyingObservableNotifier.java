// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.feedobservable;

import org.chromium.base.Consumer;

/**
 * A FeedObservable implementation that allows calling a notify method on all observers, and
 * immediately triggers observers when they are registered.
 */
public final class ImmediatelyNotifyingObservableNotifier<ObserverT>
        extends FeedObservable<ObserverT> {
    private Consumer<ObserverT> mCurrentConsumer;

    public ImmediatelyNotifyingObservableNotifier(Consumer<ObserverT> listenerMethod) {
        mCurrentConsumer = listenerMethod;
    }

    @Override
    public void registerObserver(ObserverT observerT) {
        super.registerObserver(observerT);
        mCurrentConsumer.accept(observerT);
    }

    /** Calls all the registered listeners using the given listener method. */
    public void notifyListeners(Consumer<ObserverT> listenerMethod) {
        mCurrentConsumer = listenerMethod;
        synchronized (mObservers) {
            for (ObserverT listener : mObservers) {
                listenerMethod.accept(listener);
            }
        }
    }
}
