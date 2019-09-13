// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.ntp.cards.ItemViewType;
import org.chromium.chrome.browser.ntp.cards.NewTabPageAdapter;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Adds touchless specific logic that needs to be attached and/or interacted with view holders.
 */
public class TouchlessNewTabPageAdapter extends NewTabPageAdapter {
    private final PropertyModel mModel;
    private final FocusableComponent mFocusableOpenLastTab;
    private final FocusableComponent mFocusableSiteSuggestions;
    private final Callback<View> mAsyncFocusDelegate;

    // Initial focus should only be set once. After that happens this sentinel is sent to false and
    // the initial focus field should not be used.
    private boolean mCheckInitialFocus = true;

    private static final String VIEW_HOLDER_BUNDLE_KEY = "VHBK";
    private static final String VIEW_HOLDER_TYPE_KEY = "VHTK";
    private static final String ADAPTER_POSITION_KEY = "APK";

    /**
     * Creates the adapter that will manage all the cards to display on the NTP.
     * @param uiDelegate used to interact with the rest of the system.
     * @param aboveTheFoldView the layout encapsulating all the above-the-fold elements
     *         (logo, search box, most visited tiles), or null if only suggestions should
     *         be displayed.
     * @param uiConfig the NTP UI configuration, to be passed to created views.
     * @param offlinePageBridge used to determine if articles are available.
     * @param contextMenuManager used to build context menus.
     * @param model the model of the recycler view component.
     * @param focusableOpenLastTab Focusable object for the last tab component.
     * @param focusableSiteSuggestions Focusable object for the site suggestions component.
     * @param asyncFocusDelegate Used to safely request focus for a given view.
     */
    public TouchlessNewTabPageAdapter(SuggestionsUiDelegate uiDelegate,
            @Nullable View aboveTheFoldView, UiConfig uiConfig, OfflinePageBridge offlinePageBridge,
            ContextMenuManager contextMenuManager, PropertyModel model,
            FocusableComponent focusableOpenLastTab, FocusableComponent focusableSiteSuggestions,
            Callback<View> asyncFocusDelegate) {
        super(uiDelegate, aboveTheFoldView, uiConfig, offlinePageBridge, contextMenuManager);
        mModel = model;
        mFocusableOpenLastTab = focusableOpenLastTab;
        mFocusableSiteSuggestions = focusableSiteSuggestions;
        mAsyncFocusDelegate = asyncFocusDelegate;
    }

    @Override
    public NewTabPageViewHolder onCreateViewHolder(ViewGroup parent, @ItemViewType int viewType) {
        switch (viewType) {
            case ItemViewType.ABOVE_THE_FOLD:
                return new AboveTheFoldViewHolder(
                        mAboveTheFoldView, mFocusableOpenLastTab, mFocusableSiteSuggestions);
            case ItemViewType.ACTION:
                return new TouchlessActionItemViewHolder(
                        mRecyclerView, mContextMenuManager, mUiDelegate, mUiConfig);
            case ItemViewType.SNIPPET:
                return new TouchlessArticleViewHolder(mRecyclerView, mContextMenuManager,
                        mUiDelegate, mUiConfig, mOfflinePageBridge, mAsyncFocusDelegate);
            default:
                return super.onCreateViewHolder(parent, viewType);
        }
    }

    @Override
    public void onBindViewHolder(NewTabPageViewHolder holder, int position, List<Object> payloads) {
        super.onBindViewHolder(holder, position, payloads);
        Callback<Bundle> focusCallback =
                mModel.get(TouchlessNewTabPageProperties.FOCUS_CHANGE_CALLBACK);
        if (focusCallback != null) {
            holder.setOnFocusListener((Bundle holderBundle) -> {
                Bundle wrappedBundle = new Bundle();
                wrappedBundle.putInt(ADAPTER_POSITION_KEY, holder.getAdapterPosition());
                wrappedBundle.putBundle(VIEW_HOLDER_BUNDLE_KEY, holderBundle);
                wrappedBundle.putString(VIEW_HOLDER_TYPE_KEY, holder.getClass().getName());
                focusCallback.onResult(wrappedBundle);
                // If focus is changing due to user interactions, the initial focus is no longer
                // needed. While this flag is typically flipped when the initial focus is set,
                // if the initial focus is not restored successfully, for example when the
                // initial focus would be on an index larger than the initial number of cards,
                // then it needs to be cleared here.
                invalidateFocusRestore();
            });
        }

        Bundle focusToRestore = mModel.get(TouchlessNewTabPageProperties.INITIAL_FOCUS);
        if (canRestoreFocus() && focusToRestore != null
                && focusToRestore.getInt(ADAPTER_POSITION_KEY) == holder.getAdapterPosition()
                && holder.getClass().getName().equals(
                        focusToRestore.getString(VIEW_HOLDER_TYPE_KEY))) {
            holder.requestFocusWithBundle(focusToRestore.getBundle(VIEW_HOLDER_BUNDLE_KEY));
            invalidateFocusRestore();
        }
    }

    private boolean canRestoreFocus() {
        return mCheckInitialFocus;
    }

    private void invalidateFocusRestore() {
        mCheckInitialFocus = false;
    }
}