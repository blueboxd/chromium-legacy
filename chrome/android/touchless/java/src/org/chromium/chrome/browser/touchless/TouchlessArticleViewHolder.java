// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.os.Bundle;
import android.view.View;

import androidx.annotation.LayoutRes;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticleViewHolder;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.suggestions.SuggestionsBinder;
import org.chromium.chrome.browser.suggestions.SuggestionsRecyclerView;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;
import org.chromium.chrome.touchless.R;

/**
 * A class that represents the view for a single card snippet.
 */
public class TouchlessArticleViewHolder extends SnippetArticleViewHolder {
    private final Callback<View> mAsyncFocusDelegate;

    /**
     * Constructs a {@link TouchlessArticleViewHolder} item used to display snippets.
     * @param parent The SuggestionsRecyclerView that is going to contain the newly created view.
     * @param contextMenuManager The manager responsible for the context menu.
     * @param uiDelegate The delegate object used to open an article, fetch thumbnails, etc.
     * @param uiConfig The NTP UI configuration object used to adjust the article UI.
     * @param offlinePageBridge used to determine if article is prefetched.
     * @param asyncFocusDelegate used to request focus when views are guaranteed to be focusable.
     */
    public TouchlessArticleViewHolder(SuggestionsRecyclerView parent,
            ContextMenuManager contextMenuManager, SuggestionsUiDelegate uiDelegate,
            UiConfig uiConfig, OfflinePageBridge offlinePageBridge,
            Callback<View> asyncFocusDelegate) {
        super(parent, contextMenuManager, uiDelegate, uiConfig, offlinePageBridge, getLayout());
        mAsyncFocusDelegate = asyncFocusDelegate;
    }

    @Override
    public void requestFocusWithBundle(Bundle focusBundle) {
        mAsyncFocusDelegate.onResult(itemView);
    }

    @Override
    public void setOnFocusListener(Callback<Bundle> callback) {
        itemView.setOnFocusChangeListener((View view, boolean hasFocus) -> {
            if (hasFocus) {
                callback.onResult(null);
            }
        });
    }

    @LayoutRes
    private static int getLayout() {
        return R.layout.touchless_content_suggestions_card;
    }

    @Override
    protected SuggestionsBinder createBinder(SuggestionsUiDelegate uiDelegate) {
        return new TouchlessSuggestionsBinder(itemView, uiDelegate);
    }
}
