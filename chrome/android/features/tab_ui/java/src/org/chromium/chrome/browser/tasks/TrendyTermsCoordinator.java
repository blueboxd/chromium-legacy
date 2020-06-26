// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.chromium.chrome.browser.tasks.TrendyTermsCoordinator.ItemType.TRENDY_TERMS;

import android.content.Context;
import android.view.LayoutInflater;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.widget.ViewLookupCachingFrameLayout;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Coordinator that manages trendy terms in start surface.
 */
public class TrendyTermsCoordinator {
    @IntDef({TRENDY_TERMS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ItemType {
        int TRENDY_TERMS = 0;
    }

    private final MVCListAdapter.ModelList mListItems;

    TrendyTermsCoordinator(Context context, RecyclerView recyclerView) {
        mListItems = new MVCListAdapter.ModelList();
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(mListItems);
        adapter.registerType(TRENDY_TERMS, parent -> {
            ViewLookupCachingFrameLayout view =
                    (ViewLookupCachingFrameLayout) LayoutInflater.from(context).inflate(
                            R.layout.trendy_terms_item, parent, false);
            view.setClickable(true);
            return view;
        }, TrendyTermsViewBinder::bind);
        recyclerView.setAdapter(adapter);
        recyclerView.setLayoutManager(
                new LinearLayoutManager(context, LinearLayoutManager.HORIZONTAL, false));
    }

    @VisibleForTesting
    void addTrendyTermForTesting(@ItemType int type, PropertyModel model) {
        mListItems.add(new SimpleRecyclerViewAdapter.ListItem(type, model));
    }
}
