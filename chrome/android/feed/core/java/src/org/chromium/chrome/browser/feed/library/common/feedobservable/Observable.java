// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.feedobservable;

/** Allows for registering and unregistering observers */
public interface Observable<ObserverT> {
    /** Register a new observer. If already registered, will have no effect. */
    void registerObserver(ObserverT observer);

    /** Unregister an observer. If not registered, will have no effect. */
    void unregisterObserver(ObserverT observer);
}
