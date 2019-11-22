// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.common.feedobservable;

import com.google.android.libraries.feed.common.functional.Consumer;

/**
 * A FeedObservable implementation that allows calling a notify method on all observers, and
 * immediately triggers observers when they are registered.
 */
public final class ImmediatelyNotifyingObservableNotifier<ObserverT>
        extends FeedObservable<ObserverT> {
    private Consumer<ObserverT> currentConsumer;

    public ImmediatelyNotifyingObservableNotifier(Consumer<ObserverT> listenerMethod) {
        currentConsumer = listenerMethod;
    }

    @Override
    public void registerObserver(ObserverT observerT) {
        super.registerObserver(observerT);
        currentConsumer.accept(observerT);
    }

    /** Calls all the registered listeners using the given listener method. */
    public void notifyListeners(Consumer<ObserverT> listenerMethod) {
        currentConsumer = listenerMethod;
        synchronized (observers) {
            for (ObserverT listener : observers) {
                listenerMethod.accept(listener);
            }
        }
    }
}
