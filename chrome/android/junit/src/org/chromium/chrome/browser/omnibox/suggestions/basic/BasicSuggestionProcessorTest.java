// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.support.v4.util.ArrayMap;

import androidx.annotation.IntDef;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.SuggestionDrawableState;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties.SuggestionIcon;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;

/**
 * Tests for {@link BasicSuggestionProcessor}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BasicSuggestionProcessorTest {
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
                    put(OmniboxSuggestionType.CLIPBOARD_URL, "CLIPBOARD_URL");
                    put(OmniboxSuggestionType.VOICE_SUGGEST, "VOICE_SUGGEST");
                    put(OmniboxSuggestionType.DOCUMENT_SUGGESTION, "DOCUMENT_SUGGESTION");
                    put(OmniboxSuggestionType.PEDAL, "PEDAL");
                    put(OmniboxSuggestionType.CLIPBOARD_TEXT, "CLIPBOARD_TEXT");
                    put(OmniboxSuggestionType.CLIPBOARD_IMAGE, "CLIPBOARD_IMAGE");
                    // Note: CALCULATOR suggestions are not handled by basic suggestion processor.
                    // These suggestions are now processed by AnswerSuggestionProcessor instead.
                }
            };

    @Rule
    public TestRule mFeatureProcessor = new Features.JUnitProcessor();

    @Mock
    SuggestionHost mSuggestionHost;
    @Mock
    LargeIconBridge mIconBridge;
    @Mock
    UrlBarEditingTextStateProvider mUrlBarText;
    @Mock
    Bitmap mFakeBitmap;

    private Activity mActivity;
    private BasicSuggestionProcessor mProcessor;
    private OmniboxSuggestion mSuggestion;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        doReturn("").when(mUrlBarText).getTextWithoutAutocomplete();
        mProcessor = new BasicSuggestionProcessor(
                mActivity, mSuggestionHost, mUrlBarText, () -> mIconBridge);
    }

    /**
     * Create Suggestion for test.
     * Do not use directly; use helper methods to create specific suggestion type instead.
     */
    private void createSuggestion(int type, boolean isSearch, boolean isBookmark, String title,
            String description, String url) {
        mSuggestion = new OmniboxSuggestion(type,
                /* isSearchType */ isSearch, /* relevance */ 0, /* transition */ 0, title,
                /* displayTextClassifications */ new ArrayList<>(), description,
                /* descriptionClassifications */ new ArrayList<>(),
                /* suggestionAnswer */ null, /* fillIntoEdit */ null, url,
                /* imageUrl */ "", /* imageDominantColor */ "", isBookmark,
                /* isDeletable */ false);
        mModel = mProcessor.createModelForSuggestion(mSuggestion);
        mProcessor.populateModel(mSuggestion, mModel, 0);
    }

    /** Create bookmark suggestion for test. */
    private void createBookmarkSuggestion(int type, String title, String description, String url) {
        createSuggestion(type, false, true, title, description, url);
    }

    /** Create search suggestion for test. */
    private void createSearchSuggestion(int type, String title, String description) {
        createSuggestion(type, true, false, title, description, null);
    }

    /** Create URL suggestion for test. */
    private void createUrlSuggestion(int type, String title, String description, String url) {
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

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_SHOW_SUGGESTION_FAVICONS)
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
                {OmniboxSuggestionType.CLIPBOARD_URL, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.VOICE_SUGGEST, SuggestionIcon.VOICE},
                {OmniboxSuggestionType.DOCUMENT_SUGGESTION, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.PEDAL, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.CLIPBOARD_TEXT, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.CLIPBOARD_IMAGE, SuggestionIcon.MAGNIFIER},
        };

        mProcessor.onNativeInitialized();
        for (int[] testCase : testCases) {
            createSearchSuggestion(testCase[0], "", "");
            Assert.assertTrue(mModel.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION));
            assertSuggestionTypeAndIcon(testCase[0], testCase[1]);
        }
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_SHOW_SUGGESTION_FAVICONS)
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
                {OmniboxSuggestionType.CLIPBOARD_URL, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.VOICE_SUGGEST, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.DOCUMENT_SUGGESTION, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.PEDAL, SuggestionIcon.GLOBE},
        };

        mProcessor.onNativeInitialized();
        for (int[] testCase : testCases) {
            createUrlSuggestion(testCase[0], "", "", null);
            Assert.assertFalse(mModel.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION));
            assertSuggestionTypeAndIcon(testCase[0], testCase[1]);
        }
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_SHOW_SUGGESTION_FAVICONS)
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
                {OmniboxSuggestionType.CLIPBOARD_URL, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.VOICE_SUGGEST, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.DOCUMENT_SUGGESTION, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.PEDAL, SuggestionIcon.BOOKMARK},
        };

        mProcessor.onNativeInitialized();
        for (int[] testCase : testCases) {
            createBookmarkSuggestion(testCase[0], "", "", null);
            Assert.assertFalse(mModel.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION));
            assertSuggestionTypeAndIcon(testCase[0], testCase[1]);
        }
    }

    @Test
    public void refineIconNotShownForWhatYouTypedSuggestions() {
        final String typed = "Typed content";
        doReturn(typed).when(mUrlBarText).getTextWithoutAutocomplete();
        createSearchSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, typed, "");
        Assert.assertFalse(mProcessor.canRefine(mSuggestion));

        createUrlSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, typed, "", null);
        Assert.assertFalse(mProcessor.canRefine(mSuggestion));
    }

    @Test
    public void refineIconShownForRefineSuggestions() {
        final String typed = "Typed conte";
        final String refined = "Typed content";
        doReturn(typed).when(mUrlBarText).getTextWithoutAutocomplete();
        createSearchSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, refined, "");
        Assert.assertTrue(mProcessor.canRefine(mSuggestion));

        createUrlSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, refined, "", null);
        Assert.assertTrue(mProcessor.canRefine(mSuggestion));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_SHOW_SUGGESTION_FAVICONS)
    public void suggestionFavicons_showFaviconWhenAvailable() {
        final ArgumentCaptor<LargeIconCallback> callback =
                ArgumentCaptor.forClass(LargeIconCallback.class);
        final String url = "http://url";
        mProcessor.onNativeInitialized();
        createUrlSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, "", "", url);
        SuggestionDrawableState icon1 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon1);

        verify(mIconBridge).getLargeIconForUrl(eq(url), anyInt(), callback.capture());
        callback.getValue().onLargeIconAvailable(mFakeBitmap, 0, false, 0);
        SuggestionDrawableState icon2 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon2);

        Assert.assertNotEquals(icon1, icon2);
        Assert.assertEquals(mFakeBitmap, ((BitmapDrawable) icon2.drawable).getBitmap());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_SHOW_SUGGESTION_FAVICONS)
    public void suggestionFavicons_doNotReplaceFallbackIconWhenNoFaviconIsAvailable() {
        final ArgumentCaptor<LargeIconCallback> callback =
                ArgumentCaptor.forClass(LargeIconCallback.class);
        final String url = "http://url";
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

    @Test
    @Features.DisableFeatures(ChromeFeatureList.OMNIBOX_SHOW_SUGGESTION_FAVICONS)
    public void suggestionFavicons_showNoIconsAtAllWhenFeatureIsDisabled() {
        final ArgumentCaptor<LargeIconCallback> callback =
                ArgumentCaptor.forClass(LargeIconCallback.class);
        final String url = "http://url";
        mProcessor.onNativeInitialized();
        createUrlSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, "", "", url);
        SuggestionDrawableState icon = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNull(icon);
        verify(mIconBridge, never()).getLargeIconForUrl(eq(url), anyInt(), any());
    }
}
