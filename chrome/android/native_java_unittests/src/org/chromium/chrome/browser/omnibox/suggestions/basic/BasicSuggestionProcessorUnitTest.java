// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.drawable.BitmapDrawable;

import androidx.annotation.IntDef;
import androidx.collection.ArrayMap;

import org.junit.Assert;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.CalledByNativeJavaTest;
import org.chromium.base.annotations.NativeJavaTestFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.SuggestionDrawableState;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties.SuggestionIcon;
import org.chromium.chrome.browser.ui.favicon.LargeIconBridge;
import org.chromium.chrome.browser.ui.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;

/**
 * Tests for {@link BasicSuggestionProcessor}.
 */
public class BasicSuggestionProcessorUnitTest {
    @IntDef({SuggestionKind.URL, SuggestionKind.SEARCH, SuggestionKind.BOOKMARK})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SuggestionKind {
        int URL = 0;
        int SEARCH = 1;
        int BOOKMARK = 2;
    }

    private static final ArrayMap<Integer, String> ICON_TYPE_NAMES =
            new ArrayMap<Integer, String>(SuggestionIcon.TOTAL_COUNT) {
                {
                    put(SuggestionIcon.UNSET, "UNSET");
                    put(SuggestionIcon.BOOKMARK, "BOOKMARK");
                    put(SuggestionIcon.HISTORY, "HISTORY");
                    put(SuggestionIcon.GLOBE, "GLOBE");
                    put(SuggestionIcon.MAGNIFIER, "MAGNIFIER");
                    put(SuggestionIcon.VOICE, "VOICE");
                    put(SuggestionIcon.FAVICON, "FAVICON");
                }
            };

    private static final ArrayMap<Integer, String> SUGGESTION_TYPE_NAMES =
            new ArrayMap<Integer, String>(OmniboxSuggestionType.NUM_TYPES) {
                {
                    put(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, "URL_WHAT_YOU_TYPED");
                    put(OmniboxSuggestionType.HISTORY_URL, "HISTORY_URL");
                    put(OmniboxSuggestionType.HISTORY_TITLE, "HISTORY_TITLE");
                    put(OmniboxSuggestionType.HISTORY_BODY, "HISTORY_BODY");
                    put(OmniboxSuggestionType.HISTORY_KEYWORD, "HISTORY_KEYWORD");
                    put(OmniboxSuggestionType.NAVSUGGEST, "NAVSUGGEST");
                    put(OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED, "SEARCH_WHAT_YOU_TYPED");
                    put(OmniboxSuggestionType.SEARCH_HISTORY, "SEARCH_HISTORY");
                    put(OmniboxSuggestionType.SEARCH_SUGGEST, "SEARCH_SUGGEST");
                    put(OmniboxSuggestionType.SEARCH_SUGGEST_ENTITY, "SEARCH_SUGGEST_ENTITY");
                    put(OmniboxSuggestionType.SEARCH_SUGGEST_TAIL, "SEARCH_SUGGEST_TAIL");
                    put(OmniboxSuggestionType.SEARCH_SUGGEST_PERSONALIZED,
                            "SEARCH_SUGGEST_PERSONALIZED");
                    put(OmniboxSuggestionType.SEARCH_SUGGEST_PROFILE, "SEARCH_SUGGEST_PROFILE");
                    put(OmniboxSuggestionType.SEARCH_OTHER_ENGINE, "SEARCH_OTHER_ENGINE");
                    put(OmniboxSuggestionType.NAVSUGGEST_PERSONALIZED, "NAVSUGGEST_PERSONALIZED");
                    put(OmniboxSuggestionType.VOICE_SUGGEST, "VOICE_SUGGEST");
                    put(OmniboxSuggestionType.DOCUMENT_SUGGESTION, "DOCUMENT_SUGGESTION");
                    put(OmniboxSuggestionType.PEDAL, "PEDAL");
                    // Note: CALCULATOR suggestions are not handled by basic suggestion processor.
                    // These suggestions are now processed by AnswerSuggestionProcessor instead.
                }
            };

    @Mock
    SuggestionHost mSuggestionHost;
    @Mock
    LargeIconBridge mIconBridge;
    @Mock
    UrlBarEditingTextStateProvider mUrlBarText;

    private Bitmap mBitmap;
    private BasicSuggestionProcessor mProcessor;
    private OmniboxSuggestion mSuggestion;
    private PropertyModel mModel;

    @CalledByNative
    private BasicSuggestionProcessorUnitTest() {}

    @CalledByNative
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn("").when(mUrlBarText).getTextWithoutAutocomplete();
        mBitmap = Bitmap.createBitmap(1, 1, Config.ALPHA_8);
        mProcessor = new BasicSuggestionProcessor(ContextUtils.getApplicationContext(),
                mSuggestionHost, mUrlBarText, () -> mIconBridge);
    }

    /**
     * Create Suggestion for test.
     * Do not use directly; use helper methods to create specific suggestion type instead.
     */
    private void createSuggestion(int type, boolean isSearch, boolean isBookmark, String title,
            String description, GURL url) {
        mSuggestion = new OmniboxSuggestion(type,
                /* isSearchType */ isSearch, /* relevance */ 0, /* transition */ 0, title,
                /* displayTextClassifications */ new ArrayList<>(), description,
                /* descriptionClassifications */ new ArrayList<>(),
                /* suggestionAnswer */ null, /* fillIntoEdit */ null, url,
                /* imageUrl */ GURL.emptyGURL(), /* imageDominantColor */ "", isBookmark,
                /* isDeletable */ false, /* postContentType */ null, /* postData */ null,
                OmniboxSuggestion.INVALID_GROUP, null);
        mModel = mProcessor.createModel();
        mProcessor.populateModel(mSuggestion, mModel, 0);
    }

    /** Create bookmark suggestion for test. */
    private void createBookmarkSuggestion(int type, String title, String description, GURL url) {
        createSuggestion(type, false, true, title, description, url);
    }

    /** Create search suggestion for test. */
    private void createSearchSuggestion(int type, String title, String description) {
        createSuggestion(type, true, false, title, description, GURL.emptyGURL());
    }

    /** Create URL suggestion for test. */
    private void createUrlSuggestion(int type, String title, String description, GURL url) {
        createSuggestion(type, false, false, title, description, url);
    }

    private void assertSuggestionTypeAndIcon(
            @OmniboxSuggestionType int expectedType, @SuggestionIcon int expectedIcon) {
        int actualIcon = mModel.get(SuggestionViewProperties.SUGGESTION_ICON_TYPE);
        Assert.assertEquals(
                String.format("%s: Want Icon %s, Got %s", SUGGESTION_TYPE_NAMES.get(expectedType),
                        ICON_TYPE_NAMES.get(expectedIcon), ICON_TYPE_NAMES.get(actualIcon)),
                expectedIcon, actualIcon);
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable(ChromeFeatureList.OMNIBOX_COMPACT_SUGGESTIONS)
    public void getSuggestionIconTypeForSearch_Default() {
        int[][] testCases = {
                {OmniboxSuggestionType.URL_WHAT_YOU_TYPED, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.HISTORY_URL, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.HISTORY_TITLE, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.HISTORY_BODY, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.HISTORY_KEYWORD, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.NAVSUGGEST, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.SEARCH_HISTORY, SuggestionIcon.HISTORY},
                {OmniboxSuggestionType.SEARCH_SUGGEST, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.SEARCH_SUGGEST_ENTITY, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.SEARCH_SUGGEST_TAIL, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.SEARCH_SUGGEST_PERSONALIZED, SuggestionIcon.HISTORY},
                {OmniboxSuggestionType.SEARCH_SUGGEST_PROFILE, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.SEARCH_OTHER_ENGINE, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.NAVSUGGEST_PERSONALIZED, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.VOICE_SUGGEST, SuggestionIcon.VOICE},
                {OmniboxSuggestionType.DOCUMENT_SUGGESTION, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.PEDAL, SuggestionIcon.MAGNIFIER},
        };

        mProcessor.onNativeInitialized();
        for (int[] testCase : testCases) {
            createSearchSuggestion(testCase[0], "", "");
            Assert.assertTrue(mModel.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION));
            assertSuggestionTypeAndIcon(testCase[0], testCase[1]);
        }
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable(ChromeFeatureList.OMNIBOX_COMPACT_SUGGESTIONS)
    public void getSuggestionIconTypeForUrl_Default() {
        int[][] testCases = {
                {OmniboxSuggestionType.URL_WHAT_YOU_TYPED, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.HISTORY_URL, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.HISTORY_TITLE, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.HISTORY_BODY, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.HISTORY_KEYWORD, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.NAVSUGGEST, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.SEARCH_HISTORY, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.SEARCH_SUGGEST, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.SEARCH_SUGGEST_ENTITY, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.SEARCH_SUGGEST_TAIL, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.SEARCH_SUGGEST_PERSONALIZED, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.SEARCH_SUGGEST_PROFILE, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.SEARCH_OTHER_ENGINE, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.NAVSUGGEST_PERSONALIZED, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.VOICE_SUGGEST, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.DOCUMENT_SUGGESTION, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.PEDAL, SuggestionIcon.GLOBE},
        };

        mProcessor.onNativeInitialized();
        for (int[] testCase : testCases) {
            createUrlSuggestion(testCase[0], "", "", GURL.emptyGURL());
            Assert.assertFalse(mModel.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION));
            assertSuggestionTypeAndIcon(testCase[0], testCase[1]);
        }
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable(ChromeFeatureList.OMNIBOX_COMPACT_SUGGESTIONS)
    public void getSuggestionIconTypeForBookmarks_Default() {
        int[][] testCases = {
                {OmniboxSuggestionType.URL_WHAT_YOU_TYPED, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.HISTORY_URL, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.HISTORY_TITLE, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.HISTORY_BODY, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.HISTORY_KEYWORD, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.NAVSUGGEST, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.SEARCH_HISTORY, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.SEARCH_SUGGEST, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.SEARCH_SUGGEST_ENTITY, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.SEARCH_SUGGEST_TAIL, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.SEARCH_SUGGEST_PERSONALIZED, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.SEARCH_SUGGEST_PROFILE, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.SEARCH_OTHER_ENGINE, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.NAVSUGGEST_PERSONALIZED, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.VOICE_SUGGEST, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.DOCUMENT_SUGGESTION, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.PEDAL, SuggestionIcon.BOOKMARK},
        };

        mProcessor.onNativeInitialized();
        for (int[] testCase : testCases) {
            createBookmarkSuggestion(testCase[0], "", "", GURL.emptyGURL());
            Assert.assertFalse(mModel.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION));
            assertSuggestionTypeAndIcon(testCase[0], testCase[1]);
        }
    }

    @CalledByNativeJavaTest
    public void refineIconNotShownForWhatYouTypedSuggestions() {
        final String typed = "Typed content";
        doReturn(typed).when(mUrlBarText).getTextWithoutAutocomplete();
        createSearchSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, typed, "");
        PropertyModel model = mProcessor.createModel();
        mProcessor.populateModel(mSuggestion, model, 0);
        Assert.assertNull(mModel.get(BaseSuggestionViewProperties.ACTION_CALLBACK));

        createUrlSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, typed, "", GURL.emptyGURL());
        mProcessor.populateModel(mSuggestion, model, 0);
        Assert.assertNull(mModel.get(BaseSuggestionViewProperties.ACTION_CALLBACK));
    }

    @CalledByNativeJavaTest
    public void refineIconShownForRefineSuggestions() {
        final String typed = "Typed conte";
        final String refined = "Typed content";
        doReturn(typed).when(mUrlBarText).getTextWithoutAutocomplete();
        createSearchSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, refined, "");
        PropertyModel model = mProcessor.createModel();
        mProcessor.populateModel(mSuggestion, model, 0);
        Assert.assertNotNull(mModel.get(BaseSuggestionViewProperties.ACTION_CALLBACK));

        createUrlSuggestion(
                OmniboxSuggestionType.URL_WHAT_YOU_TYPED, refined, "", GURL.emptyGURL());
        mProcessor.populateModel(mSuggestion, model, 0);
        Assert.assertNotNull(mModel.get(BaseSuggestionViewProperties.ACTION_CALLBACK));
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable(ChromeFeatureList.OMNIBOX_COMPACT_SUGGESTIONS)
    public void suggestionFavicons_showFaviconWhenAvailable() {
        final ArgumentCaptor<LargeIconCallback> callback =
                ArgumentCaptor.forClass(LargeIconCallback.class);
        final GURL url = new GURL("http://url.com");
        mProcessor.onNativeInitialized();
        createUrlSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, "", "", url);
        SuggestionDrawableState icon1 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon1);

        verify(mIconBridge).getLargeIconForUrl(eq(url), anyInt(), callback.capture());
        callback.getValue().onLargeIconAvailable(mBitmap, 0, false, 0);
        SuggestionDrawableState icon2 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon2);

        Assert.assertNotEquals(icon1, icon2);
        Assert.assertEquals(mBitmap, ((BitmapDrawable) icon2.drawable).getBitmap());
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable(ChromeFeatureList.OMNIBOX_COMPACT_SUGGESTIONS)
    public void suggestionFavicons_doNotReplaceFallbackIconWhenNoFaviconIsAvailable() {
        final ArgumentCaptor<LargeIconCallback> callback =
                ArgumentCaptor.forClass(LargeIconCallback.class);
        final GURL url = new GURL("http://url.com");
        mProcessor.onNativeInitialized();
        createUrlSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, "", "", url);
        SuggestionDrawableState icon1 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon1);

        verify(mIconBridge).getLargeIconForUrl(eq(url), anyInt(), callback.capture());
        callback.getValue().onLargeIconAvailable(null, 0, false, 0);
        SuggestionDrawableState icon2 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon2);

        Assert.assertEquals(icon1, icon2);
    }
}
