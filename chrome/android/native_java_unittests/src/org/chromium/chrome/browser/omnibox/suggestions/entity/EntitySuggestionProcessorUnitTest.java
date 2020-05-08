// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.entity;

import static org.hamcrest.core.IsInstanceOf.instanceOf;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.anyString;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;

import org.junit.Assert;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.BaseSwitches;
import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.CalledByNativeJavaTest;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.SuggestionDrawableState;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionHost;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/**
 * Tests for {@link EntitySuggestionProcessor}.
 */
public class EntitySuggestionProcessorUnitTest {
    // These values are used with UMA to report Omnibox.RichEntity.DecorationType histograms.
    // These values are INTENTIONALLY copied here to prevent accidental updates that may
    // cause metrics to break.
    private static final int DECORATION_TYPE_ICON = 0;
    private static final int DECORATION_TYPE_COLOR = 1;
    private static final int DECORATION_TYPE_IMAGE = 2;

    @Mock
    SuggestionHost mSuggestionHost;

    @Mock
    UrlBarEditingTextStateProvider mUrlStateProvider;

    @Mock
    ImageFetcher mImageFetcher;

    private Bitmap mBitmap;
    private EntitySuggestionProcessor mProcessor;

    /**
     * Base Suggestion class that can be used for testing.
     * Holds all mechanisms that are required to processSuggestion and validate suggestions.
     */
    class SuggestionTestHelper {
        // Stores created OmniboxSuggestion
        protected final OmniboxSuggestion mSuggestion;
        // Stores PropertyModel for the suggestion.
        protected final PropertyModel mModel;

        private SuggestionTestHelper(OmniboxSuggestion suggestion, PropertyModel model) {
            mSuggestion = suggestion;
            mModel = model;
        }

        void verifyReportedType(int expectedType) {
            Assert.assertEquals(
                    expectedType, mModel.get(EntitySuggestionViewProperties.DECORATION_TYPE));
        }

        /** Get Drawable associated with the suggestion. */
        Drawable getIcon() {
            final SuggestionDrawableState state = mModel.get(BaseSuggestionViewProperties.ICON);
            return state == null ? null : state.drawable;
        }
    }

    /** Create fake Entity suggestion. */
    SuggestionTestHelper createSuggestion(
            String subject, String description, String color, GURL url) {
        OmniboxSuggestion suggestion =
                new OmniboxSuggestion(OmniboxSuggestionType.SEARCH_SUGGEST_ENTITY,
                        /* isSearchType */ true, /* relevance */ 0, /* transition */ 0, subject,
                        /* displayTextClassifications */ null, /* description */ description,
                        /* descriptionClassifications */ null,
                        /* suggestionAnswer */ null, /* fillIntoEdit */ "",
                        /* url */ GURL.emptyGURL(),
                        /* imageUrl */ url, /* imageDominantColor */ color,
                        /* isStarred */ false, /* isDeletable */ false, /* postContentType */ null,
                        /* postData */ null, OmniboxSuggestion.INVALID_GROUP, null);
        PropertyModel model = mProcessor.createModel();
        return new SuggestionTestHelper(suggestion, model);
    }

    /** Populate model for associated suggestion. */
    void processSuggestion(SuggestionTestHelper helper) {
        mProcessor.populateModel(helper.mSuggestion, helper.mModel, 0);
    }

    @CalledByNative
    private EntitySuggestionProcessorUnitTest() {}

    @CalledByNative
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        // TODO(https://crbug.com/1071892): Replace this with an annotation.
        CommandLine.init(null);
        CommandLine.getInstance().appendSwitch(BaseSwitches.DISABLE_LOW_END_DEVICE_MODE);

        mBitmap = Bitmap.createBitmap(1, 1, Config.ALPHA_8);

        mProcessor = new EntitySuggestionProcessor(
                ContextUtils.getApplicationContext(), mSuggestionHost, () -> mImageFetcher);
    }

    @CalledByNative
    public void tearDown() {
        CommandLine.reset();
    }

    @CalledByNativeJavaTest
    public void contentTest_basicContent() {
        SuggestionTestHelper suggHelper =
                createSuggestion("subject", "details", null, GURL.emptyGURL());
        processSuggestion(suggHelper);
        Assert.assertEquals(
                "subject", suggHelper.mModel.get(EntitySuggestionViewProperties.SUBJECT_TEXT));
        Assert.assertEquals(
                "details", suggHelper.mModel.get(EntitySuggestionViewProperties.DESCRIPTION_TEXT));
        suggHelper.verifyReportedType(DECORATION_TYPE_ICON);
    }

    @CalledByNativeJavaTest
    public void decorationTest_noColorOrImage() {
        SuggestionTestHelper suggHelper = createSuggestion("", "", null, GURL.emptyGURL());
        processSuggestion(suggHelper);

        Assert.assertNotNull(suggHelper.getIcon());
        Assert.assertThat(suggHelper.getIcon(), instanceOf(BitmapDrawable.class));
        suggHelper.verifyReportedType(DECORATION_TYPE_ICON);
    }

    @CalledByNativeJavaTest
    public void decorationTest_validHexColor() {
        SuggestionTestHelper suggHelper = createSuggestion("", "", "#fedcba", GURL.emptyGURL());
        processSuggestion(suggHelper);

        Assert.assertThat(suggHelper.getIcon(), instanceOf(ColorDrawable.class));
        ColorDrawable icon = (ColorDrawable) suggHelper.getIcon();
        Assert.assertEquals(icon.getColor(), 0xfffedcba);
        suggHelper.verifyReportedType(DECORATION_TYPE_COLOR);
    }

    @CalledByNativeJavaTest
    public void decorationTest_validNamedColor() {
        SuggestionTestHelper suggHelper = createSuggestion("", "", "red", GURL.emptyGURL());
        processSuggestion(suggHelper);

        Assert.assertThat(suggHelper.getIcon(), instanceOf(ColorDrawable.class));
        ColorDrawable icon = (ColorDrawable) suggHelper.getIcon();
        Assert.assertEquals(icon.getColor(), Color.RED);
        suggHelper.verifyReportedType(DECORATION_TYPE_COLOR);
    }

    @CalledByNativeJavaTest
    public void decorationTest_invalidColor() {
        // Note, fallback is the bitmap drawable representing a search loupe.
        SuggestionTestHelper suggHelper = createSuggestion("", "", "", GURL.emptyGURL());
        processSuggestion(suggHelper);
        Assert.assertThat(suggHelper.getIcon(), instanceOf(BitmapDrawable.class));
        suggHelper.verifyReportedType(DECORATION_TYPE_ICON);

        suggHelper = createSuggestion("", "", "#", GURL.emptyGURL());
        processSuggestion(suggHelper);
        Assert.assertThat(suggHelper.getIcon(), instanceOf(BitmapDrawable.class));
        suggHelper.verifyReportedType(DECORATION_TYPE_ICON);

        suggHelper = createSuggestion("", "", "invalid", GURL.emptyGURL());
        processSuggestion(suggHelper);
        Assert.assertThat(suggHelper.getIcon(), instanceOf(BitmapDrawable.class));
        suggHelper.verifyReportedType(DECORATION_TYPE_ICON);
    }

    @CalledByNativeJavaTest
    public void decorationTest_basicSuccessfulBitmapFetch() {
        final GURL url = new GURL("http://site.com");
        final ArgumentCaptor<Callback<Bitmap>> callback = ArgumentCaptor.forClass(Callback.class);

        SuggestionTestHelper suggHelper = createSuggestion("", "", "red", url);
        processSuggestion(suggHelper);

        verify(mImageFetcher)
                .fetchImage(eq(url.getSpec()), anyString(), anyInt(), anyInt(), callback.capture());
        suggHelper.verifyReportedType(DECORATION_TYPE_COLOR);
        Assert.assertThat(suggHelper.getIcon(), instanceOf(ColorDrawable.class));
        callback.getValue().onResult(mBitmap);
        Assert.assertThat(suggHelper.getIcon(), instanceOf(BitmapDrawable.class));
        Assert.assertEquals(mBitmap, ((BitmapDrawable) suggHelper.getIcon()).getBitmap());
        suggHelper.verifyReportedType(DECORATION_TYPE_IMAGE);
    }

    @CalledByNativeJavaTest
    public void decorationTest_repeatedUrlsAreFetchedOnlyOnce() {
        final GURL url1 = new GURL("http://site1.com");
        final GURL url2 = new GURL("http://site2.com");
        final SuggestionTestHelper sugg1 = createSuggestion("", "", "", url1);
        final SuggestionTestHelper sugg2 = createSuggestion("", "", "", url1);
        final SuggestionTestHelper sugg3 = createSuggestion("", "", "", url2);
        final SuggestionTestHelper sugg4 = createSuggestion("", "", "", url2);

        processSuggestion(sugg1);
        processSuggestion(sugg2);
        processSuggestion(sugg3);
        processSuggestion(sugg4);

        verify(mImageFetcher)
                .fetchImage(eq(url1.getSpec()), anyString(), anyInt(), anyInt(), any());
        verify(mImageFetcher)
                .fetchImage(eq(url2.getSpec()), anyString(), anyInt(), anyInt(), any());
        verify(mImageFetcher, times(2))
                .fetchImage(anyString(), anyString(), anyInt(), anyInt(), any());
    }

    @CalledByNativeJavaTest
    public void decorationTest_bitmapReplacesIconForAllSuggestionsWithSameUrl() {
        final GURL url = new GURL("http://site.com");
        final ArgumentCaptor<Callback<Bitmap>> callback = ArgumentCaptor.forClass(Callback.class);
        final SuggestionTestHelper sugg1 = createSuggestion("", "", "", url);
        final SuggestionTestHelper sugg2 = createSuggestion("", "", "", url);
        final SuggestionTestHelper sugg3 = createSuggestion("", "", "", url);

        processSuggestion(sugg1);
        processSuggestion(sugg2);
        processSuggestion(sugg3);

        verify(mImageFetcher)
                .fetchImage(eq(url.getSpec()), anyString(), anyInt(), anyInt(), callback.capture());

        final Drawable icon1 = sugg1.getIcon();
        final Drawable icon2 = sugg2.getIcon();
        final Drawable icon3 = sugg3.getIcon();
        sugg1.verifyReportedType(DECORATION_TYPE_ICON);
        sugg2.verifyReportedType(DECORATION_TYPE_ICON);
        sugg3.verifyReportedType(DECORATION_TYPE_ICON);

        callback.getValue().onResult(mBitmap);
        final Drawable newIcon1 = sugg1.getIcon();
        final Drawable newIcon2 = sugg2.getIcon();
        final Drawable newIcon3 = sugg3.getIcon();
        sugg1.verifyReportedType(DECORATION_TYPE_IMAGE);
        sugg2.verifyReportedType(DECORATION_TYPE_IMAGE);
        sugg3.verifyReportedType(DECORATION_TYPE_IMAGE);

        Assert.assertNotEquals(icon1, newIcon1);
        Assert.assertNotEquals(icon2, newIcon2);
        Assert.assertNotEquals(icon3, newIcon3);

        Assert.assertThat(newIcon1, instanceOf(BitmapDrawable.class));
        Assert.assertThat(newIcon2, instanceOf(BitmapDrawable.class));
        Assert.assertThat(newIcon3, instanceOf(BitmapDrawable.class));

        Assert.assertEquals(mBitmap, ((BitmapDrawable) newIcon1).getBitmap());
        Assert.assertEquals(mBitmap, ((BitmapDrawable) newIcon2).getBitmap());
        Assert.assertEquals(mBitmap, ((BitmapDrawable) newIcon3).getBitmap());
    }

    @CalledByNativeJavaTest
    public void decorationTest_failedBitmapFetchDoesNotReplaceIcon() {
        final GURL url = new GURL("http://site.com");
        final ArgumentCaptor<Callback<Bitmap>> callback = ArgumentCaptor.forClass(Callback.class);
        final SuggestionTestHelper suggHelper = createSuggestion("", "", null, url);

        processSuggestion(suggHelper);
        verify(mImageFetcher)
                .fetchImage(eq(url.getSpec()), anyString(), anyInt(), anyInt(), callback.capture());

        final Drawable oldIcon = suggHelper.getIcon();
        callback.getValue().onResult(null);
        final Drawable newIcon = suggHelper.getIcon();

        Assert.assertEquals(oldIcon, newIcon);
        Assert.assertThat(oldIcon, instanceOf(BitmapDrawable.class));
        suggHelper.verifyReportedType(DECORATION_TYPE_ICON);
    }

    @CalledByNativeJavaTest
    public void decorationTest_failedBitmapFetchDoesNotReplaceColor() {
        final GURL url = new GURL("http://site.com");
        final ArgumentCaptor<Callback<Bitmap>> callback = ArgumentCaptor.forClass(Callback.class);
        final SuggestionTestHelper suggHelper = createSuggestion("", "", "red", url);

        processSuggestion(suggHelper);
        verify(mImageFetcher)
                .fetchImage(eq(url.getSpec()), anyString(), anyInt(), anyInt(), callback.capture());

        final Drawable oldIcon = suggHelper.getIcon();
        callback.getValue().onResult(null);
        final Drawable newIcon = suggHelper.getIcon();

        Assert.assertEquals(oldIcon, newIcon);
        Assert.assertThat(oldIcon, instanceOf(ColorDrawable.class));
        suggHelper.verifyReportedType(DECORATION_TYPE_COLOR);
    }

    @CalledByNativeJavaTest
    public void decorationTest_updatedModelsAreRemovedFromPendingRequestsList() {
        ArgumentCaptor<Callback<Bitmap>> callback = ArgumentCaptor.forClass(Callback.class);
        final GURL url = new GURL("http://site1.com");

        final SuggestionTestHelper sugg1 = createSuggestion("", "", "", url);
        final SuggestionTestHelper sugg2 = createSuggestion("", "", "", url);

        processSuggestion(sugg1);
        processSuggestion(sugg2);

        verify(mImageFetcher)
                .fetchImage(eq(url.getSpec()), anyString(), anyInt(), anyInt(), callback.capture());
        verify(mImageFetcher).fetchImage(anyString(), anyString(), anyInt(), anyInt(), any());

        final Drawable icon1 = sugg1.getIcon();
        final Drawable icon2 = sugg2.getIcon();

        // Invoke callback twice. If models were not erased, these should be updated.
        callback.getValue().onResult(null);
        callback.getValue().onResult(mBitmap);

        final Drawable newIcon1 = sugg1.getIcon();
        final Drawable newIcon2 = sugg2.getIcon();

        // Observe no change, despite updated image.
        Assert.assertEquals(icon1, newIcon1);
        Assert.assertEquals(icon2, newIcon2);
        sugg1.verifyReportedType(DECORATION_TYPE_ICON);
        sugg2.verifyReportedType(DECORATION_TYPE_ICON);
    }
}
