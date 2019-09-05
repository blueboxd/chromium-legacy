// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

/**
 * Informed of interesting events that happen during the lifetime of NavigationController. This
 * interface is only notified of main frame navigations.
 *
 * The lifecycle of a navigation:
 * 1) navigationStarted()
 * 2) 0 or more navigationRedirected()
 * 3) 0 or 1 navigationCommitted()
 * 4) navigationCompleted() or navigationFailed()
 */
public abstract class NavigationObserver {
    public void navigationStarted(Navigation navigation) {}

    public void navigationRedirected(Navigation navigation) {}

    public void navigationCommitted(Navigation navigation) {}

    public void navigationCompleted(Navigation navigation) {}

    public void navigationFailed(Navigation navigation) {}
}
