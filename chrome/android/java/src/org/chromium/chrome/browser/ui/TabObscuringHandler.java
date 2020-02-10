// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.view.View;

import org.chromium.base.ObserverList;

import java.util.HashSet;
import java.util.Set;

/**
 * Passes around the ability to set a view that is obscuring all tabs.
 */
public class TabObscuringHandler {
    /**
     * Interface for the observers of the tab-obscuring state change.
     */
    public interface Observer {
        /**
         * @param isObscured {@code true} if the observer is obscured by another view.
         */
        void updateObscured(boolean isObscured);
    }

    // A set of views obscuring all tabs. When this set is nonempty, all tab content will be hidden
    // from the accessibility tree.
    private final Set<View> mViewsObscuringAllTabs = new HashSet<>();

    private final ObserverList<Observer> mVisibilityObservers = new ObserverList<>();

    /**
     * Add a view to the set of views that obscure the content of all tabs for
     * accessibility. As long as this set is nonempty, all tabs should be
     * hidden from the accessibility tree.
     *
     * @param view The view that obscures the contents of all tabs.
     */
    public void addViewObscuringAllTabs(View view) {
        mViewsObscuringAllTabs.add(view);
        notifyUpdate(isViewObscuringAllTabs());
    }

    /**
     * Remove a view that previously obscured the content of all tabs.
     *
     * @param view The view that no longer obscures the contents of all tabs.
     */
    public void removeViewObscuringAllTabs(View view) {
        mViewsObscuringAllTabs.remove(view);
        notifyUpdate(isViewObscuringAllTabs());
    }

    /** @return Whether or not any views obscure all tabs. */
    public boolean isViewObscuringAllTabs() {
        return !mViewsObscuringAllTabs.isEmpty();
    }

    /**
     * Add {@link Observer} object.
     * @param observer Observer object monitoring tab visibility.
     */
    public void addObserver(Observer observer) {
        mVisibilityObservers.addObserver(observer);
    }

    /**
     * Remove {@link Observer} object.
     * @param observer Observer object monitoring tab visibility.
     */
    public void removeObserver(Observer observer) {
        mVisibilityObservers.removeObserver(observer);
    }

    /**
     * Notify all the observers of the visibility update.
     */
    private void notifyUpdate(boolean isObscured) {
        for (Observer observer : mVisibilityObservers) {
            observer.updateObscured(isObscured);
        }
    }
}
