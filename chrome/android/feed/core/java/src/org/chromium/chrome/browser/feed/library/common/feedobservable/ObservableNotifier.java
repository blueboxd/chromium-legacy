// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.feedobservable;

import org.chromium.base.Consumer;

/** A FeedObservable implementation that allows calling a notify method on all observers. */
public class ObservableNotifier<ObserverT> extends FeedObservable<ObserverT> {
    /** Calls all the registered listeners using the given listener method. */
    public void notifyListeners(Consumer<ObserverT> listenerMethod) {
        synchronized (mObservers) {
            for (ObserverT listener : mObservers) {
                listenerMethod.accept(listener);
            }
        }
    }
}
