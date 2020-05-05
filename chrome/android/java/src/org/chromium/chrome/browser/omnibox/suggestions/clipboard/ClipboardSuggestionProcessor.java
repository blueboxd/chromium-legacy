// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.clipboard;

import android.content.Context;

import androidx.annotation.DrawableRes;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.base.SuggestionDrawableState;
import org.chromium.chrome.browser.omnibox.suggestions.base.SuggestionSpannable;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties;
import org.chromium.chrome.browser.ui.favicon.LargeIconBridge;
import org.chromium.ui.modelutil.PropertyModel;

/** A class that handles model and view creation for the clipboard suggestions. */
public class ClipboardSuggestionProcessor extends BaseSuggestionViewProcessor {
    private final Context mContext;
    private final Supplier<LargeIconBridge> mIconBridgeSupplier;
    private final int mDesiredFaviconWidthPx;

    /**
     * @param context An Android context.
     * @param suggestionHost A handle to the object using the suggestions.
     * @param iconBridgeSupplier A {@link LargeIconBridge} supplies site favicons.
     */
    public ClipboardSuggestionProcessor(Context context, SuggestionHost suggestionHost,
            Supplier<LargeIconBridge> iconBridgeSupplier) {
        super(context, suggestionHost);
        mContext = context;
        mIconBridgeSupplier = iconBridgeSupplier;
        mDesiredFaviconWidthPx = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_favicon_size);
    }

    @Override
    public boolean doesProcessSuggestion(OmniboxSuggestion suggestion) {
        return suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_URL
                || suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_TEXT
                || suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_IMAGE;
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.CLIPBOARD_SUGGESTION;
    }

    @Override
    public PropertyModel createModel() {
        return new PropertyModel(SuggestionViewProperties.ALL_KEYS);
    }

    @Override
    public void populateModel(OmniboxSuggestion suggestion, PropertyModel model, int position) {
        super.populateModel(suggestion, model, position);

        boolean isUrlSuggestion = suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_URL;

        model.set(SuggestionViewProperties.IS_SEARCH_SUGGESTION, !isUrlSuggestion);
        model.set(SuggestionViewProperties.TEXT_LINE_1_TEXT,
                new SuggestionSpannable(suggestion.getDescription()));
        model.set(SuggestionViewProperties.TEXT_LINE_2_TEXT,
                new SuggestionSpannable(suggestion.getDisplayText()));

        @DrawableRes
        final int icon =
                isUrlSuggestion ? R.drawable.ic_globe_24dp : R.drawable.ic_suggestion_magnifier;
        setSuggestionDrawableState(model,
                SuggestionDrawableState.Builder.forDrawableRes(mContext, icon)
                        .setAllowTint(true)
                        .build());

        if (isUrlSuggestion) {
            // Update favicon for URL if it is available.
            fetchSuggestionFavicon(model, suggestion.getUrl(), mIconBridgeSupplier.get(), null);
        }
    }
}
