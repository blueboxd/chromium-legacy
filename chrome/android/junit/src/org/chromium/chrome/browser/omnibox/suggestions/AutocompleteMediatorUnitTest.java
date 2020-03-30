// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.anyObject;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for {@link AutocompleteMediator}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AutocompleteMediatorUnitTest {
    private static final int MINIMUM_NUMBER_OF_SUGGESTIONS_TO_SHOW = 5;
    private static final int SUGGESTION_MIN_HEIGHT = 10;

    @Rule
    public TestRule mFeatureProcessor = new Features.JUnitProcessor();

    @Mock
    AutocompleteDelegate mAutocompleteDelegate;

    @Mock
    UrlBarEditingTextStateProvider mTextStateProvider;

    @Mock
    SuggestionProcessor mMockProcessor;

    @Mock
    AutocompleteController mAutocompleteController;

    @Mock
    ToolbarDataProvider mToolbarDataProvider;

    private Activity mActivity;
    private PropertyModel mListModel;
    private AutocompleteMediator mMediator;
    private List<OmniboxSuggestion> mSuggestionsList;
    private ModelList mSuggestionModels;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mSuggestionModels = new ModelList();
        mListModel = new PropertyModel(SuggestionListProperties.ALL_KEYS);
        mListModel.set(SuggestionListProperties.SUGGESTION_MODELS, mSuggestionModels);

        mMediator = new AutocompleteMediator(mActivity, mAutocompleteDelegate, mTextStateProvider,
                mAutocompleteController, mListModel);
        mMediator.setToolbarDataProvider(mToolbarDataProvider);
        mMediator.registerSuggestionProcessor(mMockProcessor);
        mMediator.setSuggestionVisibilityState(
                AutocompleteMediator.SuggestionVisibilityState.ALLOWED);

        when(mMockProcessor.doesProcessSuggestion(any())).thenReturn(true);
        when(mMockProcessor.createModelForSuggestion(any()))
                .thenAnswer((mock) -> new PropertyModel(SuggestionCommonProperties.ALL_KEYS));
        when(mMockProcessor.getMinimumSuggestionViewHeight()).thenReturn(SUGGESTION_MIN_HEIGHT);

        mSuggestionsList = buildDummySuggestionsList(10, "Suggestion");
    }

    /**
     * Build a fake suggestions list with elements named 'Suggestion #', where '#' is the suggestion
     * index (1-based).
     *
     * @return List of suggestions.
     */
    private List<OmniboxSuggestion> buildDummySuggestionsList(int count, String prefix) {
        List<OmniboxSuggestion> list = new ArrayList<>();

        for (int index = 0; index < count; ++index) {
            OmniboxSuggestion suggestion = new OmniboxSuggestion(
                    OmniboxSuggestionType.SEARCH_SUGGEST, true /* isSearchType */,
                    0 /* relevance */, 0 /* transition */, prefix + (index + 1),
                    null /* displayTextClassifications */, null /* description */,
                    null /* descriptionClassifications */, null /* answer */,
                    null /* fillIntoEdit */, null /* url */, null /* imageUrl */,
                    null /* imageDominantColor */, false /* isStarred */, false /* isDeletable */);
            list.add(suggestion);
        }

        return list;
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @Features.DisableFeatures(ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP)
    public void updateSuggestionsList_showsAtLeast5Suggestions() {
        mMediator.onNativeInitialized();

        final int maximumListHeight = SUGGESTION_MIN_HEIGHT * 3;

        mMediator.setNewSuggestions(mSuggestionsList);
        mMediator.updateSuggestionsList(maximumListHeight);

        Assert.assertEquals(MINIMUM_NUMBER_OF_SUGGESTIONS_TO_SHOW, mSuggestionModels.size());
        Assert.assertTrue(mListModel.get(SuggestionListProperties.VISIBLE));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @Features.DisableFeatures(ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP)
    public void updateSuggestionsList_fillsInAvailableSpace() {
        mMediator.onNativeInitialized();

        final int maximumListHeight = SUGGESTION_MIN_HEIGHT * 7;

        mMediator.setNewSuggestions(mSuggestionsList);
        mMediator.updateSuggestionsList(maximumListHeight);

        Assert.assertEquals(7, mSuggestionModels.size());
        Assert.assertTrue(mListModel.get(SuggestionListProperties.VISIBLE));
    }

    @Test
    @Features.DisableFeatures({
        ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
        ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP
    })
    public void updateSuggestionsList_notEffectiveWhenDisabled() {
        mMediator.onNativeInitialized();

        final int maximumListHeight = SUGGESTION_MIN_HEIGHT * 2;

        mMediator.setNewSuggestions(mSuggestionsList);
        mMediator.updateSuggestionsList(maximumListHeight);

        Assert.assertEquals(mSuggestionsList.size(), mSuggestionModels.size());
        Assert.assertTrue(mListModel.get(SuggestionListProperties.VISIBLE));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @Features.DisableFeatures(ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP)
    public void updateSuggestionsList_worksWithNullList() {
        mMediator.onNativeInitialized();

        final int maximumListHeight = SUGGESTION_MIN_HEIGHT * 7;

        mMediator.setNewSuggestions(null);
        mMediator.updateSuggestionsList(maximumListHeight);

        Assert.assertEquals(0, mSuggestionModels.size());
        Assert.assertFalse(mListModel.get(SuggestionListProperties.VISIBLE));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @Features.DisableFeatures(ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP)
    public void updateSuggestionsList_worksWithEmptyList() {
        mMediator.onNativeInitialized();

        final int maximumListHeight = SUGGESTION_MIN_HEIGHT * 7;

        mMediator.setNewSuggestions(new ArrayList<>());
        mMediator.updateSuggestionsList(maximumListHeight);

        Assert.assertEquals(0, mSuggestionModels.size());
        Assert.assertFalse(mListModel.get(SuggestionListProperties.VISIBLE));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @Features.DisableFeatures(ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP)
    public void updateSuggestionsList_reusesExistingSuggestionsWhenHinted() {
        mMediator.onNativeInitialized();

        mMediator.setNewSuggestions(mSuggestionsList);
        mMediator.updateSuggestionsList(SUGGESTION_MIN_HEIGHT * 5);

        // Verify that processor has only been initialized once.
        verify(mMockProcessor, times(1))
                .populateModel(eq(mSuggestionsList.get(0)), anyObject(), eq(0));
        verify(mMockProcessor, times(1))
                .populateModel(eq(mSuggestionsList.get(1)), anyObject(), eq(1));
        verify(mMockProcessor, times(1))
                .populateModel(eq(mSuggestionsList.get(2)), anyObject(), eq(2));
        verify(mMockProcessor, times(1))
                .populateModel(eq(mSuggestionsList.get(3)), anyObject(), eq(3));
        verify(mMockProcessor, times(1))
                .populateModel(eq(mSuggestionsList.get(4)), anyObject(), eq(4));

        // Simulate list growing by 1 element. Verify that we have not re-initialized all elements,
        // but only added the final one to the list.
        mMediator.updateSuggestionsList(SUGGESTION_MIN_HEIGHT * 6);

        // Verify that previous suggestions have not been re-initialized.
        verify(mMockProcessor, times(1))
                .populateModel(eq(mSuggestionsList.get(5)), anyObject(), eq(5));
        verify(mMockProcessor, times(6)).populateModel(anyObject(), anyObject(), anyInt());

        Assert.assertEquals(6, mSuggestionModels.size());
        Assert.assertTrue(mListModel.get(SuggestionListProperties.VISIBLE));
    }

    @Test
    public void setNewSuggestions_detectsSameSuggestions() {
        final List<OmniboxSuggestion> list1 = buildDummySuggestionsList(5, "Set A");
        final List<OmniboxSuggestion> list2 = buildDummySuggestionsList(5, "Set A");
        mMediator.setNewSuggestions(list1);
        Assert.assertFalse(mMediator.setNewSuggestions(list2));
    }

    @Test
    public void setNewSuggestions_detectsDifferentLists() {
        final List<OmniboxSuggestion> list1 = buildDummySuggestionsList(5, "Set A");
        final List<OmniboxSuggestion> list2 = buildDummySuggestionsList(5, "Set B");
        mMediator.setNewSuggestions(list1);
        Assert.assertTrue(mMediator.setNewSuggestions(list2));
    }

    @Test
    public void setNewSuggestions_detectsNewElements() {
        final List<OmniboxSuggestion> list1 = buildDummySuggestionsList(5, "Set A");
        final List<OmniboxSuggestion> list2 = buildDummySuggestionsList(6, "Set A");
        mMediator.setNewSuggestions(list1);
        Assert.assertTrue(mMediator.setNewSuggestions(list2));
    }

    @Test
    public void setNewSuggestions_detectsNoDifferenceWhenSuppliedEmptyLists() {
        final List<OmniboxSuggestion> list1 = new ArrayList<>();
        final List<OmniboxSuggestion> list2 = new ArrayList<>();
        mMediator.setNewSuggestions(list1);
        Assert.assertFalse(mMediator.setNewSuggestions(list2));
    }

    @Test
    public void setNewSuggestions_detectsDifferenceWhenWorkingWithEmptyLists() {
        final List<OmniboxSuggestion> list1 = buildDummySuggestionsList(5, "Set");
        final List<OmniboxSuggestion> list2 = new ArrayList<>();
        mMediator.setNewSuggestions(list1);
        // Changing from populated list to an empty list.
        Assert.assertTrue(mMediator.setNewSuggestions(list2));
        // Changing from an empty list to populated list.
        Assert.assertTrue(mMediator.setNewSuggestions(list1));
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP)
    public void updateSuggestionsList_defersKeyboardPopupWhenHaveLotsOfSuggestionsToShow() {
        mMediator.onNativeInitialized();
        mMediator.signalPendingKeyboardShowDecision();
        mMediator.onSuggestionsReceived(mSuggestionsList, "");
        verify(mAutocompleteDelegate, times(1)).setKeyboardVisibility(eq(false));
        verify(mAutocompleteDelegate, never()).setKeyboardVisibility(eq(true));
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP)
    public void updateSuggestionsList_showsKeyboardWhenHaveFewSuggestionsToShow() {
        mMediator.onNativeInitialized();
        mMediator.signalPendingKeyboardShowDecision();
        mMediator.onSuggestionsReceived(
                mSuggestionsList.subList(0, MINIMUM_NUMBER_OF_SUGGESTIONS_TO_SHOW), "");
        verify(mAutocompleteDelegate, times(1)).setKeyboardVisibility(eq(true));
        verify(mAutocompleteDelegate, never()).setKeyboardVisibility(eq(false));
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP)
    public void updateSuggestionsList_doesNotShowKeyboardAfterReceivingSubsequentSuggestionLists() {
        mMediator.onNativeInitialized();
        mMediator.signalPendingKeyboardShowDecision();
        mMediator.onSuggestionsReceived(mSuggestionsList, "");
        mMediator.onSuggestionsReceived(mSuggestionsList.subList(0, 1), "");
        verify(mAutocompleteDelegate, times(1)).setKeyboardVisibility(eq(false));
        verify(mAutocompleteDelegate, never()).setKeyboardVisibility(eq(true));
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP)
    public void updateSuggestionsList_hidesKeyboardOnScrollWithLotsOfSuggestions() {
        // It is quite important that we hide keyboard every time the user initiates scroll.
        // The reason for this is that the keyboard could be requested by the user when they press
        // the omnibox field. This is beyond our control.
        mMediator.onNativeInitialized();
        mMediator.signalPendingKeyboardShowDecision();
        mMediator.onSuggestionsReceived(mSuggestionsList, "");
        verify(mAutocompleteDelegate, times(1)).setKeyboardVisibility(eq(false));
        // Should request keyboard hide.
        mMediator.onSuggestionListScroll();
        verify(mAutocompleteDelegate, times(2)).setKeyboardVisibility(eq(false));
        // Should also request keyboard hide.
        mMediator.onSuggestionListScroll();
        verify(mAutocompleteDelegate, times(3)).setKeyboardVisibility(eq(false));
        verify(mAutocompleteDelegate, never()).setKeyboardVisibility(eq(true));
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP)
    public void updateSuggestionsList_retainsKeyboardOnScrollWithFewSuggestions() {
        mMediator.onNativeInitialized();
        mMediator.signalPendingKeyboardShowDecision();
        mMediator.onSuggestionsReceived(
                mSuggestionsList.subList(0, MINIMUM_NUMBER_OF_SUGGESTIONS_TO_SHOW), "");
        verify(mAutocompleteDelegate, times(1)).setKeyboardVisibility(eq(true));

        // Should perform no action.
        mMediator.onSuggestionListScroll();
        // Should perform no action.
        mMediator.onSuggestionListScroll();

        verify(mAutocompleteDelegate, never()).setKeyboardVisibility(eq(false));
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP})
    public void onTextChanged_emptyTextTriggersZeroSuggest() {
        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        Profile profile = Mockito.mock(Profile.class);
        String url = "http://www.example.com";
        String title = "Title";
        int pageClassification = 2;
        when(mToolbarDataProvider.getProfile()).thenReturn(profile);
        when(mToolbarDataProvider.getCurrentUrl()).thenReturn(url);
        when(mToolbarDataProvider.getTitle()).thenReturn(title);
        when(mToolbarDataProvider.hasTab()).thenReturn(true);
        when(mToolbarDataProvider.getPageClassification(false)).thenReturn(pageClassification);

        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("");

        mMediator.onNativeInitialized();
        mMediator.onTextChanged("", "");
        verify(mAutocompleteController)
                .startZeroSuggest(profile, "", url, pageClassification, title);
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP})
    public void onTextChanged_nonEmptyTextTriggersSuggestions() {
        Profile profile = Mockito.mock(Profile.class);
        String url = "http://www.example.com";
        int pageClassification = 2;
        when(mToolbarDataProvider.getProfile()).thenReturn(profile);
        when(mToolbarDataProvider.getCurrentUrl()).thenReturn(url);
        when(mToolbarDataProvider.hasTab()).thenReturn(true);
        when(mToolbarDataProvider.getPageClassification(false)).thenReturn(pageClassification);

        when(mTextStateProvider.shouldAutocomplete()).thenReturn(true);
        when(mTextStateProvider.getSelectionStart()).thenReturn(4);
        when(mTextStateProvider.getSelectionEnd()).thenReturn(4);

        mMediator.onNativeInitialized();
        mMediator.onTextChanged("test", "testing");
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mAutocompleteController).start(profile, url, pageClassification, "test", 4, false);
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP})
    public void onTextChanged_cancelsPendingRequests() {
        Profile profile = Mockito.mock(Profile.class);
        String url = "http://www.example.com";
        int pageClassification = 2;
        when(mToolbarDataProvider.getProfile()).thenReturn(profile);
        when(mToolbarDataProvider.getCurrentUrl()).thenReturn(url);
        when(mToolbarDataProvider.hasTab()).thenReturn(true);
        when(mToolbarDataProvider.getPageClassification(false)).thenReturn(pageClassification);

        when(mTextStateProvider.shouldAutocomplete()).thenReturn(true);
        when(mTextStateProvider.getSelectionStart()).thenReturn(4);
        when(mTextStateProvider.getSelectionEnd()).thenReturn(4);

        mMediator.onNativeInitialized();
        mMediator.onTextChanged("test", "testing");
        mMediator.onTextChanged("nottest", "nottesting");
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mAutocompleteController)
                .start(profile, url, pageClassification, "nottest", 4, false);
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP})
    public void onSuggestionsReceived_sendsOnSuggestionsChanged() {
        mMediator.onNativeInitialized();
        mMediator.onSuggestionsReceived(mSuggestionsList, "inline_autocomplete");
        verify(mAutocompleteDelegate).onSuggestionsChanged("inline_autocomplete");

        // Ensure duplicate requests are suppressed.
        mMediator.onSuggestionsReceived(mSuggestionsList, "inline_autocomplete2");
        verifyNoMoreInteractions(mAutocompleteDelegate);
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP})
    public void setLayoutDirection_beforeInitialization() {
        mMediator.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        mMediator.setNewSuggestions(mSuggestionsList);
        mMediator.updateSuggestionsList(Integer.MAX_VALUE);
        Assert.assertEquals(mSuggestionsList.size(), mSuggestionModels.size());
        for (int i = 0; i < mSuggestionModels.size(); i++) {
            Assert.assertEquals(i + "th model does not have the expected layout direction.",
                    View.LAYOUT_DIRECTION_RTL,
                    mSuggestionModels.get(i).model.get(
                            SuggestionCommonProperties.LAYOUT_DIRECTION));
        }
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP})
    public void setLayoutDirection_afterInitialization() {
        mMediator.setNewSuggestions(mSuggestionsList);
        mMediator.updateSuggestionsList(Integer.MAX_VALUE);
        Assert.assertEquals(mSuggestionsList.size(), mSuggestionModels.size());

        mMediator.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        for (int i = 0; i < mSuggestionModels.size(); i++) {
            Assert.assertEquals(i + "th model does not have the expected layout direction.",
                    View.LAYOUT_DIRECTION_RTL,
                    mSuggestionModels.get(i).model.get(
                            SuggestionCommonProperties.LAYOUT_DIRECTION));
        }

        mMediator.setLayoutDirection(View.LAYOUT_DIRECTION_LTR);
        for (int i = 0; i < mSuggestionModels.size(); i++) {
            Assert.assertEquals(i + "th model does not have the expected layout direction.",
                    View.LAYOUT_DIRECTION_LTR,
                    mSuggestionModels.get(i).model.get(
                            SuggestionCommonProperties.LAYOUT_DIRECTION));
        }
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP})
    public void onUrlFocusChange_triggersZeroSuggest_nativeInitialized() {
        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        Profile profile = Mockito.mock(Profile.class);
        String url = "http://www.example.com";
        String title = "Title";
        int pageClassification = 2;
        when(mToolbarDataProvider.getProfile()).thenReturn(profile);
        when(mToolbarDataProvider.getCurrentUrl()).thenReturn(url);
        when(mToolbarDataProvider.getTitle()).thenReturn(title);
        when(mToolbarDataProvider.hasTab()).thenReturn(true);
        when(mToolbarDataProvider.getPageClassification(false)).thenReturn(pageClassification);

        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn(url);

        mMediator.onNativeInitialized();
        mMediator.onUrlFocusChange(true);
        verify(mAutocompleteController)
                .startZeroSuggest(profile, url, url, pageClassification, title);
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP})
    public void onUrlFocusChange_triggersZeroSuggest_nativeNotInitialized() {
        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        Profile profile = Mockito.mock(Profile.class);
        String url = "http://www.example.com";
        String title = "Title";
        int pageClassification = 2;
        when(mToolbarDataProvider.getProfile()).thenReturn(profile);
        when(mToolbarDataProvider.getCurrentUrl()).thenReturn(url);
        when(mToolbarDataProvider.getTitle()).thenReturn(title);
        when(mToolbarDataProvider.hasTab()).thenReturn(true);
        when(mToolbarDataProvider.getPageClassification(false)).thenReturn(pageClassification);

        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("");

        // Signal focus prior to initializing native.
        mMediator.onUrlFocusChange(true);

        // Initialize native and ensure zero suggest is triggered.
        mMediator.onNativeInitialized();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mAutocompleteController)
                .startZeroSuggest(profile, "", url, pageClassification, title);
    }
}
