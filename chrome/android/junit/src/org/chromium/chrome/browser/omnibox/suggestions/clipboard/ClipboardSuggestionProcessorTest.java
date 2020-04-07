// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.clipboard;

import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.view.View;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.SuggestionDrawableState;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewViewBinder;
import org.chromium.chrome.browser.ui.favicon.LargeIconBridge;
import org.chromium.chrome.browser.ui.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;

/**
 * Tests for {@link ClipboardSuggestionProcessor}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ClipboardSuggestionProcessorTest {
    private static final String TEST_URL = "http://url";
    private static final String ARABIC_URL = "http://site.com/مرحبا/123";
    private static final String ARABIC_STRING = "مرحبامرحبامرحبا";

    @Mock
    SuggestionHost mSuggestionHost;
    @Mock
    LargeIconBridge mIconBridge;
    @Mock
    Bitmap mFakeBitmap;
    @Mock
    View mRootView;
    @Mock
    TextView mTitleTextView;
    @Mock
    TextView mContentTextView;
    @Mock
    Resources mResources;

    private Activity mActivity;
    private ClipboardSuggestionProcessor mProcessor;
    private OmniboxSuggestion mSuggestion;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mProcessor =
                new ClipboardSuggestionProcessor(mActivity, mSuggestionHost, () -> mIconBridge);
        when(mRootView.getContext()).thenReturn(mActivity);
        when(mRootView.findViewById(R.id.line_1)).thenReturn(mTitleTextView);
        when(mRootView.findViewById(R.id.line_2)).thenReturn(mContentTextView);
        when(mRootView.getResources()).thenReturn(mResources);
    }

    /** Create clipboard suggestion for test. */
    private void createClipboardSuggestion(int type, String url) {
        mSuggestion = new OmniboxSuggestion(type,
                /* isSearchType */ type != OmniboxSuggestionType.CLIPBOARD_URL, /* relevance */ 0,
                /* transition */ 0, "test title",
                /* displayTextClassifications */ new ArrayList<>(), "test description",
                /* descriptionClassifications */ new ArrayList<>(),
                /* suggestionAnswer */ null, /* fillIntoEdit */ null, url,
                /* imageUrl */ "", /* imageDominantColor */ "", false,
                /* isDeletable */ false, /* postContentType */ null, /* postData */ null);
        mModel = mProcessor.createModelForSuggestion(mSuggestion);
        mProcessor.populateModel(mSuggestion, mModel, 0);
        SuggestionViewViewBinder.bind(mModel, mRootView, SuggestionViewProperties.TEXT_LINE_1_TEXT);
        SuggestionViewViewBinder.bind(
                mModel, mRootView, SuggestionCommonProperties.USE_DARK_COLORS);
        SuggestionViewViewBinder.bind(
                mModel, mRootView, SuggestionViewProperties.IS_SEARCH_SUGGESTION);
        SuggestionViewViewBinder.bind(mModel, mRootView, SuggestionViewProperties.TEXT_LINE_2_TEXT);
    }

    @Test
    public void clipboardSugestion_identifyUrlSuggestion() {
        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_URL, "");
        Assert.assertFalse(mModel.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION));
        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_TEXT, "");
        Assert.assertTrue(mModel.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION));
        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_IMAGE, "");
        Assert.assertTrue(mModel.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION));
    }

    @Test
    public void clipboardSuggestion_doesNotRefine() {
        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_URL, "");
        Assert.assertFalse(mProcessor.canRefine(mSuggestion));

        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_TEXT, "");
        Assert.assertFalse(mProcessor.canRefine(mSuggestion));

        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_IMAGE, "");
        Assert.assertFalse(mProcessor.canRefine(mSuggestion));
    }

    @Test
    public void clipboardSuggestion_showsFaviconWhenAvailable() {
        final ArgumentCaptor<LargeIconCallback> callback =
                ArgumentCaptor.forClass(LargeIconCallback.class);
        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_URL, TEST_URL);
        SuggestionDrawableState icon1 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon1);

        verify(mIconBridge).getLargeIconForStringUrl(eq(TEST_URL), anyInt(), callback.capture());
        callback.getValue().onLargeIconAvailable(mFakeBitmap, 0, false, 0);
        SuggestionDrawableState icon2 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon2);

        Assert.assertNotEquals(icon1, icon2);
        Assert.assertEquals(mFakeBitmap, ((BitmapDrawable) icon2.drawable).getBitmap());
    }

    @Test
    public void clipboardSuggestion_showsFallbackIconWhenNoFaviconIsAvailable() {
        final ArgumentCaptor<LargeIconCallback> callback =
                ArgumentCaptor.forClass(LargeIconCallback.class);
        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_URL, TEST_URL);
        SuggestionDrawableState icon1 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon1);

        verify(mIconBridge).getLargeIconForStringUrl(eq(TEST_URL), anyInt(), callback.capture());
        callback.getValue().onLargeIconAvailable(null, 0, false, 0);
        SuggestionDrawableState icon2 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon2);

        Assert.assertEquals(icon1, icon2);
    }

    @Test
    public void clipobardSuggestion_urlAndTextDirection() {
        final ArgumentCaptor<LargeIconCallback> callback =
                ArgumentCaptor.forClass(LargeIconCallback.class);
        // URL
        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_URL, ARABIC_URL);
        Assert.assertFalse(mModel.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION));
        verify(mIconBridge).getLargeIconForStringUrl(eq(ARABIC_URL), anyInt(), callback.capture());
        callback.getValue().onLargeIconAvailable(null, 0, false, 0);
        verify(mContentTextView).setTextDirection(eq(TextView.TEXT_DIRECTION_LTR));

        // Text
        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_TEXT, "");
        verify(mContentTextView).setTextDirection(eq(TextView.TEXT_DIRECTION_INHERIT));
    }
}
