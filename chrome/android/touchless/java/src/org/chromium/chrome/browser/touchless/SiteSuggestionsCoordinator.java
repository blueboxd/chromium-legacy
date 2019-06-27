// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.content.Context;
import android.graphics.Rect;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.view.ViewStub;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.ImageFetcher;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.chrome.touchless.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.RecyclerViewAdapter;

/**
 * Controller for the carousel version of most likely, to be shown on touchless devices.
 */
class SiteSuggestionsCoordinator {
    static final PropertyModel.WritableIntPropertyKey CURRENT_INDEX_KEY =
            new PropertyModel.WritableIntPropertyKey();
    static final PropertyModel.WritableIntPropertyKey INITIAL_INDEX_KEY =
            new PropertyModel.WritableIntPropertyKey();
    static final PropertyModel
            .ReadableObjectPropertyKey<PropertyListModel<PropertyModel, PropertyKey>>
                    SUGGESTIONS_KEY = new PropertyModel.ReadableObjectPropertyKey<>();
    static final PropertyModel.WritableIntPropertyKey ITEM_COUNT_KEY =
            new PropertyModel.WritableIntPropertyKey();
    static final PropertyModel.WritableObjectPropertyKey<PropertyModel> REMOVAL_KEY =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<Runnable> ON_FOCUS_CALLBACK =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableBooleanPropertyKey SHOULD_FOCUS_VIEW =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel
            .ReadableObjectPropertyKey<Callback<View>> ASYNC_FOCUS_DELEGATE =
            new PropertyModel.ReadableObjectPropertyKey<>();

    private SiteSuggestionsMediator mMediator;

    SiteSuggestionsCoordinator(View parentView, Profile profile,
            SuggestionsNavigationDelegate navigationDelegate, ContextMenuManager contextMenuManager,
            ImageFetcher imageFetcher, TouchlessLayoutManager touchlessLayoutManager) {
        Context context = parentView.getContext();
        PropertyModel model =
                new PropertyModel
                        .Builder(CURRENT_INDEX_KEY, INITIAL_INDEX_KEY, SUGGESTIONS_KEY,
                                ITEM_COUNT_KEY, REMOVAL_KEY, ON_FOCUS_CALLBACK, SHOULD_FOCUS_VIEW,
                                ASYNC_FOCUS_DELEGATE)
                        .with(SUGGESTIONS_KEY, new PropertyListModel<>())
                        .with(ITEM_COUNT_KEY, 1)
                        .with(ASYNC_FOCUS_DELEGATE,
                                touchlessLayoutManager.createCallbackToSetViewToFocus())
                        .build();
        View suggestionsView =
                ((ViewStub) parentView.findViewById(R.id.most_likely_stub)).inflate();

        SiteSuggestionsLayoutManager layoutManager =
                new SiteSuggestionsLayoutManager(context, model);

        RoundedIconGenerator iconGenerator = ViewUtils.createDefaultRoundedIconGenerator(
                context.getResources(), /* circularIcon = */ true);
        int iconSize =
                context.getResources().getDimensionPixelSize(R.dimen.tile_view_icon_min_size);

        RecyclerView recyclerView =
                suggestionsView.findViewById(R.id.most_likely_launcher_recycler);
        SiteSuggestionsAdapter adapterDelegate = new SiteSuggestionsAdapter(model,
                navigationDelegate, contextMenuManager, layoutManager,
                suggestionsView.findViewById(R.id.most_likely_web_title_text));

        RecyclerViewAdapter<SiteSuggestionsViewHolderFactory.SiteSuggestionsViewHolder, PropertyKey>
                adapter = new RecyclerViewAdapter<>(
                        adapterDelegate, new SiteSuggestionsViewHolderFactory());

        // Add spacing because tile margins get swallowed/overridden somehow.
        // TODO(chili): use layout margin.
        recyclerView.addItemDecoration(new RecyclerView.ItemDecoration() {
            @Override
            public void getItemOffsets(
                    Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
                outRect.bottom = context.getResources().getDimensionPixelSize(
                        R.dimen.most_likely_carousel_edge_spacer);
                outRect.left = 0;
                outRect.right = 0;
                outRect.top = context.getResources().getDimensionPixelSize(
                        R.dimen.most_likely_carousel_edge_spacer);
            }
        });
        recyclerView.setLayoutManager(layoutManager);
        recyclerView.setAdapter(adapter);

        mMediator =
                new SiteSuggestionsMediator(model, profile, imageFetcher, iconGenerator, iconSize);
    }

    public void destroy() {
        mMediator.destroy();
    }

    public FocusableComponent getFocusableComponent() {
        return mMediator;
    }
}
