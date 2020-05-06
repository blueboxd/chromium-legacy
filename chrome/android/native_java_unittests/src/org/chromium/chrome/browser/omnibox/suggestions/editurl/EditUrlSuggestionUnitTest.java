// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.editurl;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import org.junit.Assert;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.CalledByNativeJavaTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewDelegate;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.favicon.LargeIconBridge;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/**
 * Unit tests for the "edit url" omnibox suggestion.
 */
public final class EditUrlSuggestionUnitTest {
    private static final String TEST_TITLE = "Test Page";
    private static final String FOOBAR_SEARCH_TERMS = "foobar";
    private static final String BARBAZ_SEARCH_TERMS = "barbaz";

    private final GURL mTestUrl = new GURL("http://www.example.com");
    private final GURL mFoobarSearchUrl =
            new GURL("http://www.example.com?q=" + FOOBAR_SEARCH_TERMS);
    private final GURL mBarbazSearchUrl =
            new GURL("http://www.example.com?q=" + BARBAZ_SEARCH_TERMS);
    private EditUrlSuggestionProcessor mProcessor;
    private PropertyModel mModel;

    @Mock
    Context mContext;

    @Mock
    Resources mResources;

    @Mock
    private ActivityTabProvider mTabProvider;

    @Mock
    private ShareDelegate mShareDelegate;

    @Mock
    private Tab mTab;

    @Mock
    private OmniboxSuggestion mWhatYouTypedSuggestion;

    @Mock
    private OmniboxSuggestion mOtherSuggestion;

    @Mock
    private OmniboxSuggestion mSearchSuggestion;

    @Mock
    private EditUrlSuggestionProcessor.LocationBarDelegate mLocationBarDelegate;

    @Mock
    private View mEditButton;

    @Mock
    private View mSuggestionView;

    @Mock
    private LargeIconBridge mIconBridge;

    @Mock
    private TemplateUrlService mTemplateUrlService;

    @Mock
    private SuggestionHost mSuggestionHost;

    @Mock
    private SuggestionViewDelegate mDelegate;

    @CalledByNative
    private EditUrlSuggestionUnitTest() {}

    @CalledByNative
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);

        when(mContext.getResources()).thenReturn(mResources);
        when(mTab.getUrl()).thenReturn(mTestUrl);
        when(mTab.getTitle()).thenReturn(TEST_TITLE);
        when(mTab.isNativePage()).thenReturn(false);
        when(mTab.isIncognito()).thenReturn(false);

        when(mTabProvider.get()).thenReturn(mTab);

        when(mWhatYouTypedSuggestion.getType())
                .thenReturn(OmniboxSuggestionType.URL_WHAT_YOU_TYPED);
        when(mWhatYouTypedSuggestion.getUrl()).thenReturn(mTestUrl);

        when(mSearchSuggestion.getType()).thenReturn(OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED);
        when(mSearchSuggestion.getUrl()).thenReturn(mFoobarSearchUrl);
        when(mSearchSuggestion.getFillIntoEdit()).thenReturn(FOOBAR_SEARCH_TERMS);

        when(mOtherSuggestion.getType()).thenReturn(OmniboxSuggestionType.SEARCH_HISTORY);

        when(mSuggestionHost.createSuggestionViewDelegate(any(), anyInt())).thenReturn(mDelegate);

        mModel = new PropertyModel.Builder(EditUrlSuggestionProperties.ALL_KEYS).build();

        mProcessor = new EditUrlSuggestionProcessor(
                mContext, mSuggestionHost, mLocationBarDelegate, () -> mIconBridge);
        mProcessor.setActivityTabProvider(mTabProvider);
        mProcessor.setShareDelegateSupplier(() -> mShareDelegate);

        when(mEditButton.getId()).thenReturn(R.id.url_edit_icon);
    }

    /** Test that the suggestion is triggered. */
    @CalledByNativeJavaTest
    public void testSuggestionTriggered() {
        mProcessor.onUrlFocusChange(true);

        Assert.assertTrue("The processor should handle the \"what you typed\" suggestion.",
                mProcessor.doesProcessSuggestion(mWhatYouTypedSuggestion));

        mProcessor.populateModel(mWhatYouTypedSuggestion, mModel, 0);

        Assert.assertEquals("The model should have the title set.", TEST_TITLE,
                mModel.get(EditUrlSuggestionProperties.TITLE_TEXT));

        Assert.assertEquals("The model should have the URL set to the tab's URL",
                mTestUrl.getSpec(), mModel.get(EditUrlSuggestionProperties.URL_TEXT));
    }

    /** Test that the suggestion is not triggered if its url doesn't match the current page's. */
    @CalledByNativeJavaTest
    public void testWhatYouTypedWrongUrl() {
        mProcessor.onUrlFocusChange(true);

        when(mWhatYouTypedSuggestion.getUrl()).thenReturn(mFoobarSearchUrl);
        Assert.assertFalse("The processor should not handle the suggestion.",
                mProcessor.doesProcessSuggestion(mWhatYouTypedSuggestion));
    }

    /** Test the edit button is pressed, the correct method in the URL bar delegate is triggered. */
    @CalledByNativeJavaTest
    public void testEditButtonPress() {
        mProcessor.onUrlFocusChange(true);
        mProcessor.doesProcessSuggestion(mWhatYouTypedSuggestion);
        mProcessor.populateModel(mWhatYouTypedSuggestion, mModel, 0);

        mModel.get(EditUrlSuggestionProperties.BUTTON_CLICK_LISTENER).onClick(mEditButton);

        verify(mLocationBarDelegate).setOmniboxEditingText(mTestUrl.getSpec());
    }

    /** Test that when suggestion is tapped, it still navigates to the correct location. */
    @CalledByNativeJavaTest
    public void testPressSuggestion() {
        mProcessor.onUrlFocusChange(true);
        mProcessor.doesProcessSuggestion(mWhatYouTypedSuggestion);
        mProcessor.populateModel(mWhatYouTypedSuggestion, mModel, 0);

        mModel.get(EditUrlSuggestionProperties.TEXT_CLICK_LISTENER).onClick(mSuggestionView);

        verify(mDelegate).onSelection();
    }

    @CalledByNativeJavaTest
    public void testSearchSuggestion() {
        when(mTab.getUrl()).thenReturn(mFoobarSearchUrl);
        mProcessor.onUrlFocusChange(true);
        when(mTemplateUrlService.getSearchQueryForUrl(mFoobarSearchUrl))
                .thenReturn(FOOBAR_SEARCH_TERMS);
        when(mTemplateUrlService.getSearchQueryForUrl(mBarbazSearchUrl))
                .thenReturn(BARBAZ_SEARCH_TERMS);

        Assert.assertTrue(mProcessor.doesProcessSuggestion(mSearchSuggestion));

        when(mSearchSuggestion.getUrl()).thenReturn(mBarbazSearchUrl);
        when(mSearchSuggestion.getFillIntoEdit()).thenReturn(BARBAZ_SEARCH_TERMS);

        Assert.assertFalse(mProcessor.doesProcessSuggestion(mSearchSuggestion));
    }
}
