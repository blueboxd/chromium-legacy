// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionHost;
import org.chromium.chrome.browser.ui.favicon.LargeIconBridge;
import org.chromium.chrome.browser.ui.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;

/**
 * Tests for {@link BaseSuggestionViewProcessor}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BaseSuggestionProcessorTest {
    private class TestBaseSuggestionProcessor extends BaseSuggestionViewProcessor {
        private final Context mContext;
        private final LargeIconBridge mLargeIconBridge;
        private final Runnable mRunable;
        public TestBaseSuggestionProcessor(Context context, SuggestionHost suggestionHost,
                LargeIconBridge largeIconBridge, Runnable runable) {
            super(context, suggestionHost);
            mContext = context;
            mLargeIconBridge = largeIconBridge;
            mRunable = runable;
        }

        @Override
        public PropertyModel createModelForSuggestion(OmniboxSuggestion suggestion) {
            return new PropertyModel(BaseSuggestionViewProperties.ALL_KEYS);
        }

        @Override
        public boolean doesProcessSuggestion(OmniboxSuggestion suggestion) {
            return true;
        }

        @Override
        public int getViewTypeId() {
            return OmniboxSuggestionUiType.DEFAULT;
        }

        @Override
        public void populateModel(OmniboxSuggestion suggestion, PropertyModel model, int position) {
            super.populateModel(suggestion, model, position);
            setSuggestionDrawableState(model,
                    SuggestionDrawableState.Builder.forBitmap(mContext, mFakeDefaultBitmap)
                            .build());
            fetchSuggestionFavicon(model, suggestion.getUrl(), mLargeIconBridge, mRunable);
        }
    }

    @Mock
    SuggestionHost mSuggestionHost;
    @Mock
    LargeIconBridge mIconBridge;
    @Mock
    Runnable mRunnable;
    @Mock
    Bitmap mFakeBitmap;
    @Mock
    Bitmap mFakeDefaultBitmap;

    private Activity mActivity;
    private TestBaseSuggestionProcessor mProcessor;
    private OmniboxSuggestion mSuggestion;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mProcessor =
                new TestBaseSuggestionProcessor(mActivity, mSuggestionHost, mIconBridge, mRunnable);
    }

    /**
     * Create Suggestion for test.
     */
    private void createSuggestion(int type, String url) {
        mSuggestion = new OmniboxSuggestion(type,
                /* isSearchType */ false, /* relevance */ 0, /* transition */ 0, "title",
                /* displayTextClassifications */ new ArrayList<>(), "description",
                /* descriptionClassifications */ new ArrayList<>(),
                /* suggestionAnswer */ null, /* fillIntoEdit */ null, url,
                /* imageUrl */ "", /* imageDominantColor */ "", false,
                /* isDeletable */ false);
        mModel = mProcessor.createModelForSuggestion(mSuggestion);
        mProcessor.populateModel(mSuggestion, mModel, 0);
    }

    @Test
    public void suggestionFavicons_showFaviconWhenAvailable() {
        final ArgumentCaptor<LargeIconCallback> callback =
                ArgumentCaptor.forClass(LargeIconCallback.class);
        final String url = "http://url";
        createSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, url);
        SuggestionDrawableState icon1 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon1);

        verify(mIconBridge).getLargeIconForStringUrl(eq(url), anyInt(), callback.capture());
        callback.getValue().onLargeIconAvailable(mFakeBitmap, 0, false, 0);
        SuggestionDrawableState icon2 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon2);

        Assert.assertNotEquals(icon1, icon2);
        Assert.assertEquals(mFakeBitmap, ((BitmapDrawable) icon2.drawable).getBitmap());
    }

    @Test
    public void suggestionFavicons_doNotReplaceFallbackIconWhenNoFaviconIsAvailable() {
        final ArgumentCaptor<LargeIconCallback> callback =
                ArgumentCaptor.forClass(LargeIconCallback.class);
        final String url = "http://url";
        createSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, url);
        SuggestionDrawableState icon1 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon1);

        verify(mIconBridge).getLargeIconForStringUrl(eq(url), anyInt(), callback.capture());
        callback.getValue().onLargeIconAvailable(null, 0, false, 0);
        SuggestionDrawableState icon2 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon2);

        Assert.assertEquals(icon1, icon2);
    }
}
