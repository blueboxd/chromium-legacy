// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.helper;

import android.view.View;

import org.chromium.ui.modelutil.ForwardingListObservable;
import org.chromium.ui.modelutil.ListModel;

// TODO(fhorschig): Ask org.chromium.ui.modelutil OWNERS whether this should be shared.
/**
 * Observes a {@link ListModel} and forwards any changes. It implements the
 * {@link ListViewAdapter.Delegate} so it can be used to bind views as needed by a
 * {@link android.widget.ListView} that ends up displaying the observed Entries.
 *
 * @param <T> The item type inside the passed ListModel. It's passed to the {@link ViewBinder}.
 * @param <V> The view type of a single list entry.
 */
public class SimpleListViewMcp<T, V extends View>
        extends ForwardingListObservable<Void> implements ListViewAdapter.Delegate<T, V> {
    private final ListModel<T> mModel;
    private final ViewBinder<T, V> mViewBinder;

    /**
     * Subclasses of this interface bind a given item to a given view.
     * @param <T> The item type of a list entry.
     * @param <V> The view type of a single list entry.
     */
    public interface ViewBinder<T, V extends View> {
        /**
         * Binds a given item to a given view. That means, the item properties are mapped to view
         * properties. No logic should happen in here.
         * @param view The view that holds the data of a list entry.
         * @param item The item describing a single list entry.
         */
        void bind(V view, T item);
    }

    /**
     * This creates a new Model Change Processor that observes the given model.
     * @param model The {@Link ListModel} to be observed.
     * @param viewBinder The {@link ViewBinder} used to bind items to views inside the ListView.
     */
    public SimpleListViewMcp(ListModel<T> model, ViewBinder<T, V> viewBinder) {
        mModel = model;
        mViewBinder = viewBinder;
        model.addObserver(this);
    }

    @Override
    public void onBindView(V view, T item) {
        mViewBinder.bind(view, item);
    }

    @Override
    public T getItemAt(int pos) {
        return mModel.get(pos);
    }

    @Override
    public int getItemCount() {
        return mModel.size();
    }
}