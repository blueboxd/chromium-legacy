// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles.list;

import android.content.Context;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.ui.modelutil.ForwardingListObservable;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.RecyclerViewAdapter;

/**
 * The View component of a query tiles.  This takes the {@link TileListModel} and creates the
 * glue to display it on the screen.
 */
class TileListView {
    private final TileListModel mModel;
    private final RecyclerView mView;
    private final LinearLayoutManager mLayoutManager;

    /** Constructor. */
    public TileListView(Context context, TileListModel model) {
        mModel = model;
        mView = new RecyclerView(context);
        mView.setHasFixedSize(true);

        mLayoutManager = new LinearLayoutManager(context, LinearLayoutManager.HORIZONTAL, false);
        mView.setLayoutManager(mLayoutManager);

        PropertyModelChangeProcessor.create(
                mModel.getProperties(), mView, new TileListPropertyViewBinder());

        RecyclerViewAdapter<TileViewHolder, Void> adapter =
                new RecyclerViewAdapter<TileViewHolder, Void>(
                        new ModelChangeProcessor(mModel), TileViewHolder::create);
        mView.setAdapter(adapter);
        mView.post(adapter::notifyDataSetChanged);
    }

    /** @return The Android {@link View} representing this widget. */
    public View getView() {
        return mView;
    }

    private static class ModelChangeProcessor extends ForwardingListObservable<Void>
            implements RecyclerViewAdapter.Delegate<TileViewHolder, Void> {
        private final TileListModel mModel;

        public ModelChangeProcessor(TileListModel model) {
            mModel = model;
            model.addObserver(this);
        }

        @Override
        public int getItemCount() {
            return mModel.size();
        }

        @Override
        public int getItemViewType(int position) {
            return 0;
        }

        @Override
        public void onBindViewHolder(
                TileViewHolder viewHolder, int position, @Nullable Void payload) {
            viewHolder.bind(mModel.getProperties(), mModel.get(position));
        }

        @Override
        public void onViewRecycled(TileViewHolder viewHolder) {
            viewHolder.recycle();
        }
    }
}
