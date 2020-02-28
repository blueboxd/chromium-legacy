// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

import android.view.View;
import android.view.ViewGroup;

import java.util.Map;

/**
 * Interface to provide native views to incorporate in an external surface-controlled
 * RecyclerView.
 *
 * Models after a RecyclerView.Adapter.
 */
public interface ListContentManager {
    /** Returns whether the item at index is a native view or not. */
    boolean isNativeView(int index);

    /** Gets the bytes needed to render an external view. */
    byte[] getExternalViewBytes(int index);

    /** Returns map of values which should go in the context of an external view. */
    Map<String, Object> getContextValues(int index);

    /**
     * Returns the inflated native view.
     *
     * View should not be attached to parent. {@link bindNativeView} will
     * be called later to attach more information to the view.
     */
    View getNativeView(int index, ViewGroup parent);

    /**
     * Binds the data at the specified location.
     */
    void bindNativeView(int index, View v);

    /** Returns number of items to show. */
    int getItemCount();

    /** Adds an observer to be notified when the list content changes. */
    void addObserver(ListContentManagerObserver o);

    /** Removes the observer so it's no longer notified of content changes. */
    void removeObserver(ListContentManagerObserver o);
}
