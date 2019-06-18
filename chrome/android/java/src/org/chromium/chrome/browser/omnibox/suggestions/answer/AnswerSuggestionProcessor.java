// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import android.content.Context;
import android.graphics.Bitmap;
import android.util.Pair;
import android.util.TypedValue;
import android.view.View;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherConfig;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherFactory;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.answer.AnswerSuggestionViewProperties.AnswerIcon;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties.SuggestionIcon;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties.SuggestionTextContainer;
import org.chromium.chrome.browser.util.ConversionUtils;
import org.chromium.components.omnibox.AnswerType;
import org.chromium.components.omnibox.SuggestionAnswer;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** A class that handles model and view creation for the most commonly used omnibox suggestion. */
public class AnswerSuggestionProcessor implements SuggestionProcessor {
    private static final int MAX_CACHE_SIZE = 500 * ConversionUtils.BYTES_PER_KILOBYTE;

    private final Map<String, List<PropertyModel>> mPendingAnswerRequestUrls;
    private final Context mContext;
    private final SuggestionHost mSuggestionHost;
    private ImageFetcher mImageFetcher;
    private final UrlBarEditingTextStateProvider mUrlBarEditingTextProvider;
    private boolean mEnableNewAnswerLayout;

    /**
     * @param context An Android context.
     * @param suggestionHost A handle to the object using the suggestions.
     */
    public AnswerSuggestionProcessor(Context context, SuggestionHost suggestionHost,
            UrlBarEditingTextStateProvider editingTextProvider) {
        mContext = context;
        mSuggestionHost = suggestionHost;
        mPendingAnswerRequestUrls = new HashMap<>();
        mUrlBarEditingTextProvider = editingTextProvider;
    }

    public void destroy() {
        if (mImageFetcher != null) {
            mImageFetcher.destroy();
            mImageFetcher = null;
        }
    }

    @Override
    public boolean doesProcessSuggestion(OmniboxSuggestion suggestion) {
        // Calculation answers are specific in a way that these are basic suggestions, but processed
        // as answers, when new answer layout is enabled.
        return suggestion.hasAnswer() || suggestion.getType() == OmniboxSuggestionType.CALCULATOR;
    }

    @Override
    public void onNativeInitialized() {
        // Experiment: controls presence of certain answer icon types.
        mEnableNewAnswerLayout =
                ChromeFeatureList.isEnabled(ChromeFeatureList.OMNIBOX_NEW_ANSWER_LAYOUT);
    }

    @Override
    public int getViewTypeId() {
        return mEnableNewAnswerLayout ? OmniboxSuggestionUiType.ANSWER_SUGGESTION
                                      : OmniboxSuggestionUiType.DEFAULT;
    }

    @Override
    public PropertyModel createModelForSuggestion(OmniboxSuggestion suggestion) {
        return mEnableNewAnswerLayout ? new PropertyModel(AnswerSuggestionViewProperties.ALL_KEYS)
                                      : new PropertyModel(SuggestionViewProperties.ALL_KEYS);
    }

    @Override
    public void populateModel(OmniboxSuggestion suggestion, PropertyModel model, int position) {
        maybeFetchAnswerIcon(suggestion, model);
        SuggestionViewDelegate delegate =
                mSuggestionHost.createSuggestionViewDelegate(suggestion, position);

        if (mEnableNewAnswerLayout) {
            setStateForNewSuggestion(model, suggestion, delegate);
        } else {
            setStateForClassicSuggestion(model, suggestion, delegate);
        }
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        // This clear is necessary for memory as well as clearing when switching to/from incognito.
        if (!hasFocus && mImageFetcher != null) {
            mImageFetcher.clear();
        }
    }

    @Override
    public void recordSuggestionPresented(OmniboxSuggestion suggestion, PropertyModel model) {
        // Note: At the time of writing this functionality, AiS was offering at most one answer to
        // any query. If this changes before the metric is expired, the code below may need either
        // revisiting or a secondary metric telling us how many answer suggestions have been shown.
        if (suggestion.hasAnswer()) {
            RecordHistogram.recordEnumeratedHistogram("Omnibox.AnswerInSuggestShown",
                    suggestion.getAnswer().getType(), AnswerType.TOTAL_COUNT);
        }
    }

    @Override
    public void recordSuggestionUsed(OmniboxSuggestion suggestion, PropertyModel model) {
        // Bookkeeping handled in C++:
        // https://cs.chromium.org/Omnibox.SuggestionUsed.AnswerInSuggest
    }

    private void maybeFetchAnswerIcon(OmniboxSuggestion suggestion, PropertyModel model) {
        ThreadUtils.assertOnUiThread();

        // Attempting to fetch answer data before we have a profile to request it for.
        if (mSuggestionHost.getCurrentProfile() == null) return;

        // Note: we also handle calculations here, which do not have answer defined.
        if (!suggestion.hasAnswer()) return;
        final String url = suggestion.getAnswer().getSecondLine().getImage();
        if (url == null) return;

        // Do not make duplicate answer image requests for the same URL (to avoid generating
        // duplicate bitmaps for the same image).
        if (mPendingAnswerRequestUrls.containsKey(url)) {
            mPendingAnswerRequestUrls.get(url).add(model);
            return;
        }

        if (mImageFetcher == null) {
            mImageFetcher = ImageFetcherFactory.createImageFetcher(
                    ImageFetcherConfig.IN_MEMORY_ONLY,
                    ((ChromeApplication) mContext.getApplicationContext()).getReferencePool(),
                    MAX_CACHE_SIZE);
        }

        List<PropertyModel> models = new ArrayList<>();
        models.add(model);
        mPendingAnswerRequestUrls.put(url, models);

        mImageFetcher.fetchImage(
                url, ImageFetcher.ANSWER_SUGGESTIONS_UMA_CLIENT_NAME, (Bitmap bitmap) -> {
                    ThreadUtils.assertOnUiThread();

                    List<PropertyModel> currentModels = mPendingAnswerRequestUrls.remove(url);
                    boolean didUpdate = false;
                    for (int i = 0; i < currentModels.size(); i++) {
                        PropertyModel currentModel = currentModels.get(i);
                        if (!mSuggestionHost.isActiveModel(currentModel)) continue;

                        if (mEnableNewAnswerLayout) {
                            model.set(AnswerSuggestionViewProperties.ANSWER_IMAGE, bitmap);
                        } else {
                            model.set(SuggestionViewProperties.ANSWER_IMAGE, bitmap);
                        }
                        didUpdate = true;
                    }
                    if (didUpdate) mSuggestionHost.notifyPropertyModelsChanged();
                });
    }

    /**
     * Sets both lines of the Omnibox suggestion in a basic Suggestion result.
     */
    private void setStateForClassicSuggestion(
            PropertyModel model, OmniboxSuggestion suggestion, SuggestionViewDelegate delegate) {
        AnswerText[] details = AnswerTextClassic.from(mContext, suggestion);

        SuggestionAnswer answer = suggestion.getAnswer();
        if (answer != null) {
            model.set(SuggestionViewProperties.HAS_ANSWER_IMAGE, answer.getSecondLine().hasImage());
        }

        model.set(SuggestionViewProperties.IS_ANSWER, true);
        model.set(SuggestionViewProperties.DELEGATE, delegate);

        model.set(SuggestionViewProperties.TEXT_LINE_1_SIZING,
                Pair.create(TypedValue.COMPLEX_UNIT_SP, details[0].mHeightSp));
        model.set(SuggestionViewProperties.TEXT_LINE_1_TEXT,
                new SuggestionTextContainer(details[0].mText));
        model.set(SuggestionViewProperties.TEXT_LINE_1_MAX_LINES, details[0].mMaxLines);
        model.set(SuggestionViewProperties.TEXT_LINE_1_TEXT_DIRECTION, View.TEXT_DIRECTION_INHERIT);

        if (details[1] != null) {
            model.set(SuggestionViewProperties.TEXT_LINE_2_SIZING,
                    Pair.create(TypedValue.COMPLEX_UNIT_SP, details[1].mHeightSp));
            model.set(SuggestionViewProperties.TEXT_LINE_2_TEXT,
                    new SuggestionTextContainer(details[1].mText));
            model.set(SuggestionViewProperties.TEXT_LINE_2_MAX_LINES, details[1].mMaxLines);
            model.set(SuggestionViewProperties.TEXT_LINE_2_TEXT_DIRECTION,
                    View.TEXT_DIRECTION_INHERIT);
        }

        model.set(SuggestionViewProperties.SUGGESTION_ICON_TYPE, SuggestionIcon.MAGNIFIER);

        model.set(SuggestionViewProperties.REFINABLE, true);
    }

    /**
     * Sets both lines of the Omnibox suggestion based on an Answers in Suggest result.
     */
    private void setStateForNewSuggestion(
            PropertyModel model, OmniboxSuggestion suggestion, SuggestionViewDelegate delegate) {
        SuggestionAnswer answer = suggestion.getAnswer();
        AnswerText[] details = AnswerTextNewLayout.from(
                mContext, suggestion, mUrlBarEditingTextProvider.getTextWithoutAutocomplete());

        model.set(AnswerSuggestionViewProperties.DELEGATE, delegate);

        model.set(AnswerSuggestionViewProperties.TEXT_LINE_1_SIZE, details[0].mHeightSp);
        model.set(AnswerSuggestionViewProperties.TEXT_LINE_2_SIZE, details[1].mHeightSp);

        model.set(AnswerSuggestionViewProperties.TEXT_LINE_1_TEXT, details[0].mText);
        model.set(AnswerSuggestionViewProperties.TEXT_LINE_2_TEXT, details[1].mText);

        model.set(AnswerSuggestionViewProperties.TEXT_LINE_1_ACCESSIBILITY_DESCRIPTION,
                details[0].mAccessibilityDescription);
        model.set(AnswerSuggestionViewProperties.TEXT_LINE_2_ACCESSIBILITY_DESCRIPTION,
                details[1].mAccessibilityDescription);

        model.set(AnswerSuggestionViewProperties.TEXT_LINE_1_MAX_LINES, details[0].mMaxLines);
        model.set(AnswerSuggestionViewProperties.TEXT_LINE_2_MAX_LINES, details[1].mMaxLines);

        @AnswerIcon
        int icon = AnswerIcon.UNDEFINED;

        if (answer != null) {
            switch (answer.getType()) {
                case AnswerType.DICTIONARY:
                    icon = AnswerIcon.DICTIONARY;
                    break;
                case AnswerType.FINANCE:
                    icon = AnswerIcon.FINANCE;
                    break;
                case AnswerType.KNOWLEDGE_GRAPH:
                    icon = AnswerIcon.KNOWLEDGE;
                    break;
                case AnswerType.SUNRISE:
                    icon = AnswerIcon.SUNRISE;
                    break;
                case AnswerType.TRANSLATION:
                    icon = AnswerIcon.TRANSLATION;
                    break;
                case AnswerType.WEATHER:
                    icon = AnswerIcon.WEATHER;
                    break;
                case AnswerType.WHEN_IS:
                    icon = AnswerIcon.EVENT;
                    break;
                case AnswerType.CURRENCY:
                    icon = AnswerIcon.CURRENCY;
                    break;
                case AnswerType.SPORTS:
                    icon = AnswerIcon.SPORTS;
            }
        } else {
            assert suggestion.getType() == OmniboxSuggestionType.CALCULATOR;
            icon = AnswerIcon.CALCULATOR;
        }

        model.set(AnswerSuggestionViewProperties.ANSWER_ICON_TYPE, icon);
    }
}
