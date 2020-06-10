// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.GlobalDiscardableReferencePool;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherConfig;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherFactory;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.answer.AnswerSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.clipboard.ClipboardSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.editurl.EditUrlSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.entity.EntitySuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.tail.TailSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.tiles.TileSuggestionProcessor;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.favicon.LargeIconBridge;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.query_tiles.QueryTile;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** Builds DropdownItemViewInfo list from AutocompleteResult for the Suggestions list. */
class DropdownItemViewInfoListBuilder {
    private static final int MAX_IMAGE_CACHE_SIZE = 500 * ConversionUtils.BYTES_PER_KILOBYTE;

    private final List<SuggestionProcessor> mPriorityOrderedSuggestionProcessors;

    private HeaderProcessor mHeaderProcessor;
    private ActivityTabProvider mActivityTabProvider;
    private Supplier<ShareDelegate> mShareDelegateSupplier;
    private ImageFetcher mImageFetcher;
    private LargeIconBridge mIconBridge;

    DropdownItemViewInfoListBuilder() {
        mPriorityOrderedSuggestionProcessors = new ArrayList<>();
    }

    /**
     * Initialize the Builder with default set of suggestion processors.
     *
     * @param context Current context.
     * @param host Component creating suggestion view delegates and responding to suggestion events.
     * @param delegate Component facilitating interactions with UI and Autocomplete mechanism.
     * @param textProvider Provider of querying/editing the Omnibox.
     * @param queryTileSuggestionCallback Callback responding to QueryTile events.
     */
    void initDefaultProcessors(Context context, SuggestionHost host, AutocompleteDelegate delegate,
            UrlBarEditingTextStateProvider textProvider,
            Callback<List<QueryTile>> queryTileSuggestionCallback) {
        assert mPriorityOrderedSuggestionProcessors.size() == 0 : "Processors already initialized.";

        final Supplier<ImageFetcher> imageFetcherSupplier = () -> mImageFetcher;
        final Supplier<LargeIconBridge> iconBridgeSupplier = () -> mIconBridge;
        final Supplier<Tab> tabSupplier =
                () -> mActivityTabProvider == null ? null : mActivityTabProvider.get();
        final Supplier<ShareDelegate> shareSupplier =
                () -> mShareDelegateSupplier == null ? null : mShareDelegateSupplier.get();

        mHeaderProcessor = new HeaderProcessor(context, host, delegate);
        registerSuggestionProcessor(new EditUrlSuggestionProcessor(
                context, host, delegate, iconBridgeSupplier, tabSupplier, shareSupplier));
        registerSuggestionProcessor(
                new AnswerSuggestionProcessor(context, host, textProvider, imageFetcherSupplier));
        registerSuggestionProcessor(
                new ClipboardSuggestionProcessor(context, host, iconBridgeSupplier));
        registerSuggestionProcessor(
                new EntitySuggestionProcessor(context, host, imageFetcherSupplier));
        registerSuggestionProcessor(new TailSuggestionProcessor(context, host));
        registerSuggestionProcessor(
                new TileSuggestionProcessor(context, queryTileSuggestionCallback));
        registerSuggestionProcessor(
                new BasicSuggestionProcessor(context, host, textProvider, iconBridgeSupplier));
    }

    void destroy() {
        if (mImageFetcher != null) {
            mImageFetcher.destroy();
            mImageFetcher = null;
        }

        if (mIconBridge != null) {
            mIconBridge.destroy();
            mIconBridge = null;
        }
    }

    /**
     * Register new processor to process OmniboxSuggestions.
     * Processors will be tried in the same order as they were added.
     *
     * @param processor SuggestionProcessor that handles OmniboxSuggestions.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void registerSuggestionProcessor(SuggestionProcessor processor) {
        mPriorityOrderedSuggestionProcessors.add(processor);
    }

    /**
     * Specify instance of the HeaderProcessor that will be used to run tests.
     *
     * @param processor Header processor used to build suggestion headers.
     */
    void setHeaderProcessorForTest(HeaderProcessor processor) {
        mHeaderProcessor = processor;
    }

    /**
     * Notify that the current User profile has changed.
     *
     * @param profile Current user profile.
     */
    void setProfile(Profile profile) {
        if (mIconBridge != null) {
            mIconBridge.destroy();
            mIconBridge = null;
        }

        if (mImageFetcher != null) {
            mImageFetcher.destroy();
            mImageFetcher = null;
        }

        mIconBridge = new LargeIconBridge(profile);
        mImageFetcher = ImageFetcherFactory.createImageFetcher(ImageFetcherConfig.IN_MEMORY_ONLY,
                GlobalDiscardableReferencePool.getReferencePool(), MAX_IMAGE_CACHE_SIZE);
    }

    /**
     * Notify that the current Share delegate supplier has changed.
     *
     * @param shareDelegateSupplier Share facility supplier.
     */
    void setShareDelegateSupplier(Supplier<ShareDelegate> shareDelegateSupplier) {
        mShareDelegateSupplier = shareDelegateSupplier;
    }

    /**
     * Specify new Activity tab provider.
     *
     * @param provider Tab provider.
     */
    void setActivityTabProvider(ActivityTabProvider provider) {
        mActivityTabProvider = provider;
    }

    /**
     * Respond to URL bar focus change.
     *
     * @param hasFocus Indicates whether URL bar is now focused.
     */
    void onUrlFocusChange(boolean hasFocus) {
        if (!hasFocus && mImageFetcher != null) {
            mImageFetcher.clear();
        }

        mHeaderProcessor.onUrlFocusChange(hasFocus);
        for (int index = 0; index < mPriorityOrderedSuggestionProcessors.size(); index++) {
            mPriorityOrderedSuggestionProcessors.get(index).onUrlFocusChange(hasFocus);
        }
    }

    /** Signals that native initialization has completed. */
    void onNativeInitialized() {
        mHeaderProcessor.onNativeInitialized();
        for (int index = 0; index < mPriorityOrderedSuggestionProcessors.size(); index++) {
            mPriorityOrderedSuggestionProcessors.get(index).onNativeInitialized();
        }
    }

    /**
     * Build ListModel for new set of Omnibox suggestions.
     *
     * @param autocompleteResult New set of suggestions.
     * @return List of DropdownItemViewInfo representing the corresponding content of the
     *          suggestions list.
     */
    List<DropdownItemViewInfo> buildDropdownViewInfoList(AutocompleteResult autocompleteResult) {
        mHeaderProcessor.onSuggestionsReceived();
        for (int index = 0; index < mPriorityOrderedSuggestionProcessors.size(); index++) {
            mPriorityOrderedSuggestionProcessors.get(index).onSuggestionsReceived();
        }

        final List<OmniboxSuggestion> newSuggestions = autocompleteResult.getSuggestionsList();
        final int newSuggestionsCount = newSuggestions.size();
        final List<DropdownItemViewInfo> viewInfoList = new ArrayList<>();

        int currentGroup = OmniboxSuggestion.INVALID_GROUP;

        for (int index = 0; index < newSuggestionsCount; index++) {
            final OmniboxSuggestion suggestion = newSuggestions.get(index);

            if (suggestion.getGroupId() != currentGroup) {
                currentGroup = suggestion.getGroupId();
                final PropertyModel model = mHeaderProcessor.createModel();
                mHeaderProcessor.populateModel(model, currentGroup,
                        autocompleteResult.getGroupHeaders().get(currentGroup));
                viewInfoList.add(new DropdownItemViewInfo(mHeaderProcessor, model, currentGroup));
            }

            final SuggestionProcessor processor = getProcessorForSuggestion(suggestion, index);
            final PropertyModel model = processor.createModel();
            processor.populateModel(suggestion, model, index);
            viewInfoList.add(new DropdownItemViewInfo(processor, model, currentGroup));
        }
        return viewInfoList;
    }

    /**
     * Search for Processor that will handle the supplied suggestion at specific position.
     *
     * @param suggestion The suggestion to be processed.
     * @param position Position of the suggestion in the list.
     */
    private SuggestionProcessor getProcessorForSuggestion(
            OmniboxSuggestion suggestion, int position) {
        for (int index = 0; index < mPriorityOrderedSuggestionProcessors.size(); index++) {
            SuggestionProcessor processor = mPriorityOrderedSuggestionProcessors.get(index);
            if (processor.doesProcessSuggestion(suggestion, position)) return processor;
        }
        assert false : "No default handler for suggestions";
        return null;
    }
}
