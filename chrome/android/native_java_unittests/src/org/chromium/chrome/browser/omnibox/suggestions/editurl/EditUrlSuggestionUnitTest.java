// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.editurl;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.ClipboardManager;
import android.content.Context;
import android.view.View;

import org.junit.Assert;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.CalledByNativeJavaTest;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.UrlBarDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties.Action;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.favicon.LargeIconBridge;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.List;

/**
 * Unit tests for the "edit url" omnibox suggestion.
 */
public final class EditUrlSuggestionUnitTest {
    private static final String TEST_TITLE = "Test Page";
    private static final String FOOBAR_SEARCH_TERMS = "foobar";
    private static final String BARBAZ_SEARCH_TERMS = "barbaz";

    private static final int ACTION_SHARE = 0;
    private static final int ACTION_COPY = 1;
    private static final int ACTION_EDIT = 2;

    private final GURL mTestUrl = new GURL("http://www.example.com");
    private final GURL mFoobarSearchUrl =
            new GURL("http://www.example.com?q=" + FOOBAR_SEARCH_TERMS);
    private final GURL mBarbazSearchUrl =
            new GURL("http://www.example.com?q=" + BARBAZ_SEARCH_TERMS);
    private EditUrlSuggestionProcessor mProcessor;
    private PropertyModel mModel;

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
    private UrlBarDelegate mUrlBarDelegate;

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

    @CalledByNative
    private EditUrlSuggestionUnitTest() {}

    @CalledByNative
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);

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

        mModel = new PropertyModel.Builder(SuggestionViewProperties.ALL_KEYS).build();

        mProcessor = new EditUrlSuggestionProcessor(ContextUtils.getApplicationContext(),
                mSuggestionHost, mUrlBarDelegate, () -> mIconBridge);

        mProcessor.setActivityTabProvider(mTabProvider);
        mProcessor.setShareDelegateSupplier(() -> mShareDelegate);
    }

    /** Test that the suggestion is triggered. */
    @CalledByNativeJavaTest
    public void testSuggestionTriggered() {
        mProcessor.onUrlFocusChange(true);

        Assert.assertTrue("The processor should handle the \"what you typed\" suggestion.",
                mProcessor.doesProcessSuggestion(mWhatYouTypedSuggestion));

        mProcessor.populateModel(mWhatYouTypedSuggestion, mModel, 0);

        Assert.assertEquals("The model should have the title set.", TEST_TITLE,
                mModel.get(SuggestionViewProperties.TEXT_LINE_1_TEXT).toString());

        Assert.assertEquals("The model should have the URL set to the tab's URL",
                mTestUrl.getSpec(),
                mModel.get(SuggestionViewProperties.TEXT_LINE_2_TEXT).toString());
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

        List<Action> actions = mModel.get(BaseSuggestionViewProperties.ACTIONS);
        Assert.assertEquals("EditUrl suggestion should have 3 action buttons.", 3, actions.size());
        actions.get(ACTION_EDIT).callback.run();
        verify(mUrlBarDelegate).setOmniboxEditingText(mTestUrl.getSpec());
    }

    /** Test the share button is pressed, we trigger the share menu. */
    @CalledByNativeJavaTest
    public void testShareButtonPress() {
        mProcessor.onUrlFocusChange(true);
        mProcessor.doesProcessSuggestion(mWhatYouTypedSuggestion);
        mProcessor.populateModel(mWhatYouTypedSuggestion, mModel, 0);

        List<Action> actions = mModel.get(BaseSuggestionViewProperties.ACTIONS);
        Assert.assertEquals("EditUrl suggestion should have 3 action buttons.", 3, actions.size());
        actions.get(ACTION_SHARE).callback.run();
        verify(mShareDelegate, times(1)).share(mTab, false /* shareDirectly */);
    }

    /** Test the copy button is pressed, we update clipboard. */
    @CalledByNativeJavaTest
    public void testCopyButtonPress() {
        mProcessor.onUrlFocusChange(true);
        mProcessor.doesProcessSuggestion(mWhatYouTypedSuggestion);
        mProcessor.populateModel(mWhatYouTypedSuggestion, mModel, 0);

        List<Action> actions = mModel.get(BaseSuggestionViewProperties.ACTIONS);

        Assert.assertEquals("EditUrl suggestion should have 3 action buttons.", 3, actions.size());
        actions.get(ACTION_COPY).callback.run();

        ClipboardManager clipboardManager =
                (ClipboardManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.CLIPBOARD_SERVICE);
        Assert.assertEquals("Clipboard content should be updated when user presses Copy button.",
                mTestUrl.getSpec(), clipboardManager.getText());
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
