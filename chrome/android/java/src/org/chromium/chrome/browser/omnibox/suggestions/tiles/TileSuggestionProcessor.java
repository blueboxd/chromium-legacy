// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.tiles;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionProcessor;
import org.chromium.components.query_tiles.QueryTile;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** A class that handles model and view creation for the query tile suggestions. */
public class TileSuggestionProcessor implements SuggestionProcessor {
    private final Callback<List<QueryTile>> mQueryTileSuggestionCallback;
    private final int mMinViewHeight;

    /**
     * Constructor.
     * @param context An Android context.
     * @param queryTileSuggestionCallback The callback to be invoked to show query tiles.
     */
    public TileSuggestionProcessor(
            Context context, Callback<List<QueryTile>> queryTileSuggestionCallback) {
        mQueryTileSuggestionCallback = queryTileSuggestionCallback;
        mMinViewHeight = context.getResources().getDimensionPixelSize(
                org.chromium.chrome.R.dimen.omnibox_suggestion_comfortable_height);
    }

    @Override
    public boolean doesProcessSuggestion(OmniboxSuggestion suggestion) {
        return suggestion.getType() == OmniboxSuggestionType.TILE_SUGGESTION;
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.TILE_SUGGESTION;
    }

    @Override
    public int getMinimumSuggestionViewHeight() {
        return mMinViewHeight;
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {}

    @Override
    public void onNativeInitialized() {}

    @Override
    public PropertyModel createModel() {
        return new PropertyModel(TileSuggestionProperties.ALL_KEYS);
    }

    @Override
    public void populateModel(OmniboxSuggestion suggestion, PropertyModel model, int position) {
        mQueryTileSuggestionCallback.onResult(suggestion.getQueryTiles());
    }

    @Override
    public void recordSuggestionPresented(OmniboxSuggestion suggestion, PropertyModel model) {
        // TODO(crbug.com/1068672): Record metrics.
    }

    @Override
    public void recordSuggestionUsed(OmniboxSuggestion suggestion, PropertyModel model) {}

    @Override
    public void onSuggestionsReceived() {}
}
