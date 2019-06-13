// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lifecycle;

/**
 * Manages registration of {@link LifecycleObserver} instances.
 */
public interface ActivityLifecycleDispatcher {
    /**
     * Registers an observer.
     * @param observer must implement one or several observer interfaces in
     * {@link org.chromium.chrome.browser.lifecycle} in order to receive corresponding events.
     */
    void register(LifecycleObserver observer);

    /**
     * Unregisters an observer.
     */
    void unregister(LifecycleObserver observer);
}
