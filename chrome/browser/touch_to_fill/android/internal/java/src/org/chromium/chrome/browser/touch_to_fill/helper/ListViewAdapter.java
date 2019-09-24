// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.helper;

import android.database.DataSetObserver;
import android.support.annotation.Nullable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListAdapter;

import org.chromium.ui.modelutil.ListObservable;

import java.util.HashSet;
import java.util.Set;

/**
 * This Adapter creates Views of type {@link V} for a {@link android.widget.ListView} and binds
 * items of type {@link T} to each of the created views. It defers most tasks to a {@link Delegate}.
 *
 * Construct with a {@link SimpleListViewMcp} to update a ListView according to a list model.
 *
 * @param <T> The item type inside the observed list. It's passed to the {@link Delegate}.
 * @param <V> The view type of a single list entry as created by the {@link ViewFactory}.
 */
public class ListViewAdapter<T, V extends View>
        implements ListObservable.ListObserver<Void>, ListAdapter {
    private final Delegate<T, V> mDelegate;
    private final ViewFactory<V> mViewFactory;
    private final Set<DataSetObserver> mObservers = new HashSet<>();

    /**
     * This delegates decouples the ListViewAdapter from the underlying model and handles all tasks
     * for which the model might be necessary.
     * @param <T> The item type inside the observed list.
     * @param <V> The view type of a single list entry as created by the {@link ViewFactory}.
     */
    public interface Delegate<T, V extends View> {
        /**
         * Returns the list item at the given position.
         * @param pos The position inside the list to retrieve the item for.
         * @return An item of type {@link T}.
         */
        T getItemAt(int pos);

        /**
         * @return The total number of items in the observed list.
         */
        int getItemCount();

        /**
         * Binds the data of the given item to the given View.
         * @param view A View inside the ListView of type {@link V}.
         * @param item An Item inside the observed list of type {@link T}.
         */
        void onBindView(V view, T item);
    }

    /**
     * Subclasses of this interface create the View that represents an item in a ListView.
     * @param <V> The type of {@link View} this factory creates.
     */
    public interface ViewFactory<V extends View> {
        /**
         * Creates a View used in a ListView and attaches it to the given parent.
         * @param parent The parent of the newly created View.
         * @return A {@link View} that represents an item in a ListView.
         */
        V create(ViewGroup parent);
    }

    /**
     * Creates a new ListViewAdapter.
     * @param delegate The {@link Delegate} to bind views and query for list information.
     * @param viewFactory The {@link ViewFactory} used to create new views for the ListView.
     */
    public ListViewAdapter(Delegate<T, V> delegate, ViewFactory<V> viewFactory) {
        mDelegate = delegate;
        mViewFactory = viewFactory;
    }

    @Override
    public void onItemMoved(ListObservable source, int curIndex, int newIndex) {
        notifyObserversChanged();
    }

    @Override
    public void onItemRangeChanged(
            ListObservable source, int index, int count, @Nullable Void payload) {
        notifyObserversChanged();
    }

    @Override
    public void onItemRangeInserted(ListObservable source, int index, int count) {
        notifyObserversChanged();
    }

    @Override
    public void onItemRangeRemoved(ListObservable source, int index, int count) {
        notifyObserversChanged();
    }

    private void notifyObserversChanged() {
        for (DataSetObserver observer : mObservers) observer.onChanged();
    }

    @Override
    public void registerDataSetObserver(DataSetObserver dataSetObserver) {
        mObservers.add(dataSetObserver);
    }

    @Override
    public void unregisterDataSetObserver(DataSetObserver dataSetObserver) {
        mObservers.remove(dataSetObserver);
    }

    @Override
    public int getCount() {
        return mDelegate.getItemCount();
    }

    @Override
    public Object getItem(int i) {
        return mDelegate.getItemAt(i);
    }

    @Override
    public long getItemId(int i) {
        return i;
    }

    @Override
    public boolean hasStableIds() {
        return false;
    }

    @Override
    @SuppressWarnings("unchecked")
    public View getView(int pos, View view, ViewGroup parent) {
        if (view == null) view = mViewFactory.create(parent);
        mDelegate.onBindView((V) view, mDelegate.getItemAt(pos));
        return view;
    }

    @Override
    public int getItemViewType(int i) {
        return 0; // Always the same view type.
    }

    @Override
    public int getViewTypeCount() {
        return 1; // Always the same view type.
    }

    @Override
    public boolean isEmpty() {
        return mDelegate.getItemCount() == 0;
    }

    @Override
    public boolean areAllItemsEnabled() {
        return true; // There are no disabled items yet.
    }

    @Override
    public boolean isEnabled(int i) {
        return true; // There are no disabled items yet.
    }
}
