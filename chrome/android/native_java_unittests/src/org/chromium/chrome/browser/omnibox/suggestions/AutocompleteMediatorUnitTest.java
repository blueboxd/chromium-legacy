// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.anyObject;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.os.Handler;
import android.os.Message;
import android.util.SparseArray;
import android.view.View;

import org.junit.Assert;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.CalledByNativeJavaTest;
import org.chromium.base.annotations.NativeJavaTestFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteMediator.DropdownItemViewInfo;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderViewProperties;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link AutocompleteMediator}.
 */
public class AutocompleteMediatorUnitTest {
    private static final int MINIMUM_NUMBER_OF_SUGGESTIONS_TO_SHOW = 5;
    private static final int SUGGESTION_MIN_HEIGHT = 10;

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

    @Mock
    Handler mHandler;

    private PropertyModel mListModel;
    private AutocompleteMediator mMediator;
    private List<OmniboxSuggestion> mSuggestionsList;
    private ModelList mSuggestionModels;

    @CalledByNative
    private AutocompleteMediatorUnitTest() {}

    @CalledByNative
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mSuggestionModels = new ModelList();
        mListModel = new PropertyModel(SuggestionListProperties.ALL_KEYS);
        mListModel.set(SuggestionListProperties.SUGGESTION_MODELS, mSuggestionModels);

        mMediator = new AutocompleteMediator(ContextUtils.getApplicationContext(),
                mAutocompleteDelegate, mTextStateProvider, mAutocompleteController, mListModel,
                mHandler);
        mMediator.setToolbarDataProvider(mToolbarDataProvider);
        mMediator.registerSuggestionProcessor(mMockProcessor);
        mMediator.setSuggestionVisibilityState(
                AutocompleteMediator.SuggestionVisibilityState.ALLOWED);

        when(mMockProcessor.doesProcessSuggestion(any())).thenReturn(true);
        when(mMockProcessor.createModel())
                .thenAnswer((mock) -> new PropertyModel(SuggestionCommonProperties.ALL_KEYS));
        when(mMockProcessor.getMinimumViewHeight()).thenReturn(SUGGESTION_MIN_HEIGHT);
        when(mMockProcessor.getViewTypeId()).thenReturn(OmniboxSuggestionUiType.DEFAULT);

        when(mHandler.sendMessageAtTime(any(Message.class), anyLong()))
                .thenAnswer(new Answer<Void>() {
                    @Override
                    public Void answer(InvocationOnMock invocation) {
                        ((Message) invocation.getArguments()[0]).getCallback().run();
                        return null;
                    }
                });

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
            list.add(OmniboxSuggestionBuilderForTest
                             .searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                             .setDisplayText(prefix + (index + 1))
                             .build());
        }

        return list;
    }

    /**
     * Build a fake group headers map with elements named 'Header #', where '#' is the group header
     * index (1-based) and 'Header' is the supplied prefix. Each header has a corresponding key
     * computed as baseKey + #.
     *
     * @param count Number of group headers to build.
     * @param baseKey Key of the first group header.
     * @param prefix Name prefix for each group.
     * @return Map of group headers (populated in random order).
     */
    private SparseArray<String> buildDummyGroupHeaders(int count, int baseKey, String prefix) {
        SparseArray<String> headers = new SparseArray<>(count);
        for (int index = 0; index < count; index++) {
            headers.put(baseKey + index, prefix + " " + (index + 1));
        }

        return headers;
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Enable(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @NativeJavaTestFeatures.Disable(ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP)
    public void updateSuggestionsList_showsAtLeast5Suggestions() {
        mMediator.onNativeInitialized();

        final int maximumListHeight = SUGGESTION_MIN_HEIGHT * 3;

        mMediator.setNewSuggestions(new AutocompleteResult(mSuggestionsList, null));
        mMediator.updateSuggestionsList(maximumListHeight);

        Assert.assertEquals(MINIMUM_NUMBER_OF_SUGGESTIONS_TO_SHOW, mSuggestionModels.size());
        Assert.assertTrue(mListModel.get(SuggestionListProperties.VISIBLE));
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Enable(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @NativeJavaTestFeatures.Disable(ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP)
    public void updateSuggestionsList_fillsInAvailableSpace() {
        mMediator.onNativeInitialized();

        final int maximumListHeight = SUGGESTION_MIN_HEIGHT * 7;

        mMediator.setNewSuggestions(new AutocompleteResult(mSuggestionsList, null));
        mMediator.updateSuggestionsList(maximumListHeight);

        Assert.assertEquals(7, mSuggestionModels.size());
        Assert.assertTrue(mListModel.get(SuggestionListProperties.VISIBLE));
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP})
    public void updateSuggestionsList_notEffectiveWhenDisabled() {
        mMediator.onNativeInitialized();

        final int maximumListHeight = SUGGESTION_MIN_HEIGHT * 2;

        mMediator.setNewSuggestions(new AutocompleteResult(mSuggestionsList, null));
        mMediator.updateSuggestionsList(maximumListHeight);

        Assert.assertEquals(mSuggestionsList.size(), mSuggestionModels.size());
        Assert.assertTrue(mListModel.get(SuggestionListProperties.VISIBLE));
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Enable(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @NativeJavaTestFeatures.Disable(ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP)
    public void updateSuggestionsList_worksWithNullList() {
        mMediator.onNativeInitialized();

        final int maximumListHeight = SUGGESTION_MIN_HEIGHT * 7;

        mMediator.setNewSuggestions(new AutocompleteResult(null, null));
        mMediator.updateSuggestionsList(maximumListHeight);

        Assert.assertEquals(0, mSuggestionModels.size());
        Assert.assertFalse(mListModel.get(SuggestionListProperties.VISIBLE));
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Enable(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @NativeJavaTestFeatures.Disable(ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP)
    public void updateSuggestionsList_worksWithEmptyList() {
        mMediator.onNativeInitialized();

        final int maximumListHeight = SUGGESTION_MIN_HEIGHT * 7;

        mMediator.setNewSuggestions(new AutocompleteResult(new ArrayList<>(), null));
        mMediator.updateSuggestionsList(maximumListHeight);

        Assert.assertEquals(0, mSuggestionModels.size());
        Assert.assertFalse(mListModel.get(SuggestionListProperties.VISIBLE));
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Enable(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @NativeJavaTestFeatures.Disable(ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP)
    public void updateSuggestionsList_reusesExistingSuggestionsWhenHinted() {
        mMediator.onNativeInitialized();

        mMediator.setNewSuggestions(new AutocompleteResult(mSuggestionsList, null));

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

        // Check that all suggestions have been initialized exactly once.
        verify(mMockProcessor, times(mSuggestionsList.size()))
                .populateModel(anyObject(), anyObject(), anyInt());

        Assert.assertEquals(6, mSuggestionModels.size());
        Assert.assertTrue(mListModel.get(SuggestionListProperties.VISIBLE));
    }

    @CalledByNativeJavaTest
    public void setNewSuggestions_detectsSameSuggestions() {
        final List<OmniboxSuggestion> list1 = buildDummySuggestionsList(5, "Set A");
        final List<OmniboxSuggestion> list2 = buildDummySuggestionsList(5, "Set A");
        mMediator.setNewSuggestions(new AutocompleteResult(list1, null));
        Assert.assertFalse(mMediator.setNewSuggestions(new AutocompleteResult(list2, null)));
    }

    @CalledByNativeJavaTest
    public void setNewSuggestions_detectsDifferentLists() {
        final List<OmniboxSuggestion> list1 = buildDummySuggestionsList(5, "Set A");
        final List<OmniboxSuggestion> list2 = buildDummySuggestionsList(5, "Set B");
        mMediator.setNewSuggestions(new AutocompleteResult(list1, null));
        Assert.assertTrue(mMediator.setNewSuggestions(new AutocompleteResult(list2, null)));
    }

    @CalledByNativeJavaTest
    public void setNewSuggestions_detectsNewElements() {
        final List<OmniboxSuggestion> list1 = buildDummySuggestionsList(5, "Set A");
        final List<OmniboxSuggestion> list2 = buildDummySuggestionsList(6, "Set A");
        mMediator.setNewSuggestions(new AutocompleteResult(list1, null));
        Assert.assertTrue(mMediator.setNewSuggestions(new AutocompleteResult(list2, null)));
    }

    @CalledByNativeJavaTest
    public void setNewSuggestions_detectsNoDifferenceWhenSuppliedEmptyLists() {
        final List<OmniboxSuggestion> list1 = new ArrayList<>();
        final List<OmniboxSuggestion> list2 = new ArrayList<>();
        mMediator.setNewSuggestions(new AutocompleteResult(list1, null));
        Assert.assertFalse(mMediator.setNewSuggestions(new AutocompleteResult(list2, null)));
    }

    @CalledByNativeJavaTest
    public void setNewSuggestions_detectsDifferenceWhenWorkingWithEmptyLists() {
        final List<OmniboxSuggestion> list1 = buildDummySuggestionsList(5, "Set");
        final List<OmniboxSuggestion> list2 = new ArrayList<>();
        mMediator.setNewSuggestions(new AutocompleteResult(list1, null));
        // Changing from populated list to an empty list.
        Assert.assertTrue(mMediator.setNewSuggestions(new AutocompleteResult(list2, null)));
        // Changing from an empty list to populated list.
        Assert.assertTrue(mMediator.setNewSuggestions(new AutocompleteResult(list1, null)));
    }

    @CalledByNativeJavaTest
    public void updateSuggestionsList_collectsNewGroupHeaders() {
        final List<OmniboxSuggestion> list = new ArrayList<>();
        final SparseArray<String> giveHeaders = buildDummyGroupHeaders(3, 1, "Header");
        mMediator.setNewSuggestions(new AutocompleteResult(list, giveHeaders));
        final SparseArray<String> haveHeaders = mMediator.getAutocompleteResult().getGroupHeaders();

        Assert.assertEquals(giveHeaders.size(), haveHeaders.size());
        for (int index = 0; index < giveHeaders.size(); index++) {
            Assert.assertEquals(giveHeaders.keyAt(index), haveHeaders.keyAt(index));
            Assert.assertEquals(giveHeaders.valueAt(index), haveHeaders.valueAt(index));
        }
    }

    @CalledByNativeJavaTest
    public void updateSuggestionsList_newHeadersOverwriteOldHeaders() {
        final List<OmniboxSuggestion> list = new ArrayList<>();
        final SparseArray<String> oldHeaders = buildDummyGroupHeaders(5, 3, "Old Header");
        final SparseArray<String> newHeaders = buildDummyGroupHeaders(3, 1, "New Header");
        mMediator.setNewSuggestions(new AutocompleteResult(list, oldHeaders));
        mMediator.setNewSuggestions(new AutocompleteResult(list, newHeaders));

        final SparseArray<String> haveHeaders = mMediator.getAutocompleteResult().getGroupHeaders();

        Assert.assertEquals(newHeaders.size(), haveHeaders.size());
        for (int index = 0; index < newHeaders.size(); index++) {
            Assert.assertEquals(newHeaders.keyAt(index), haveHeaders.keyAt(index));
            Assert.assertEquals(newHeaders.valueAt(index), haveHeaders.valueAt(index));
        }
    }

    @CalledByNativeJavaTest
    public void updateSuggestionsList_detectsSameHeaders() {
        final List<OmniboxSuggestion> list = new ArrayList<>();
        final SparseArray<String> headers1 = buildDummyGroupHeaders(3, 1, "Header");
        final SparseArray<String> headers2 = buildDummyGroupHeaders(3, 1, "Header");

        mMediator.setNewSuggestions(new AutocompleteResult(list, headers1));
        Assert.assertFalse(mMediator.setNewSuggestions(new AutocompleteResult(list, headers2)));
    }

    @CalledByNativeJavaTest
    public void updateSuggestionsList_detectsDifferentSuggestionKeys() {
        final List<OmniboxSuggestion> list = new ArrayList<>();
        final SparseArray<String> headers1 = buildDummyGroupHeaders(3, 1, "Header");
        final SparseArray<String> headers2 = buildDummyGroupHeaders(3, 2, "Header");

        mMediator.setNewSuggestions(new AutocompleteResult(list, headers1));
        Assert.assertTrue(mMediator.setNewSuggestions(new AutocompleteResult(list, headers2)));
    }

    @CalledByNativeJavaTest
    public void updateSuggestionsList_detectsDifferentSuggestionTitles() {
        final List<OmniboxSuggestion> list = new ArrayList<>();
        final SparseArray<String> headers1 = buildDummyGroupHeaders(3, 1, "HeaderA");
        final SparseArray<String> headers2 = buildDummyGroupHeaders(3, 1, "HeaderB");

        mMediator.setNewSuggestions(new AutocompleteResult(list, headers1));
        Assert.assertTrue(mMediator.setNewSuggestions(new AutocompleteResult(list, headers2)));
    }

    @CalledByNativeJavaTest
    public void updateSuggestionsList_detectsNoDifferencesWhenEmptyAndPassedEmptyLists() {
        final List<OmniboxSuggestion> list = new ArrayList<>();
        final SparseArray<String> headers1 = new SparseArray<String>();
        final SparseArray<String> headers2 = new SparseArray<String>();

        mMediator.setNewSuggestions(new AutocompleteResult(list, headers1));
        Assert.assertFalse(mMediator.setNewSuggestions(new AutocompleteResult(list, headers2)));
    }

    @CalledByNativeJavaTest
    public void updateSuggestionsList_detectsNoDifferencesWhenEmptyAndPassedNullLists() {
        final List<OmniboxSuggestion> list = new ArrayList<>();
        final SparseArray<String> headers1 = new SparseArray<String>();
        final SparseArray<String> headers2 = null;

        mMediator.setNewSuggestions(new AutocompleteResult(list, headers1));
        Assert.assertFalse(mMediator.setNewSuggestions(new AutocompleteResult(list, headers2)));
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP})
    public void
    updateSuggestionsList_buildsHeaderForFirstSuggestion() {
        mMediator.onNativeInitialized();
        final List<OmniboxSuggestion> list = new ArrayList<>();
        final SparseArray<String> headers = buildDummyGroupHeaders(1, 1, "Header");

        OmniboxSuggestion suggestion =
                OmniboxSuggestionBuilderForTest.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setGroupId(1)
                        .build();

        list.add(suggestion);
        list.add(suggestion);

        mMediator.onSuggestionsReceived(new AutocompleteResult(list, headers), "a");
        final ModelList model = mMediator.getSuggestionModelList();
        Assert.assertEquals(2 /* suggestions */ + 1 /* group headers */, model.size());

        Assert.assertEquals(
                ((DropdownItemViewInfo) model.get(0)).type, OmniboxSuggestionUiType.HEADER);
        Assert.assertEquals(
                ((DropdownItemViewInfo) model.get(1)).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(
                ((DropdownItemViewInfo) model.get(2)).type, OmniboxSuggestionUiType.DEFAULT);

        DropdownItemViewInfo header = (DropdownItemViewInfo) model.get(0);
        Assert.assertEquals(header.model.get(HeaderViewProperties.TITLE), headers.get(1));
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP})
    public void
    updateSuggestionsList_buildsHeadersOnlyWhenGroupChanges() {
        mMediator.onNativeInitialized();
        final List<OmniboxSuggestion> list = new ArrayList<>();
        final SparseArray<String> headers = buildDummyGroupHeaders(2, 1, "Header");

        OmniboxSuggestion suggestionWithNoGroup =
                OmniboxSuggestionBuilderForTest.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .build();
        OmniboxSuggestion suggestionForGroup1 =
                OmniboxSuggestionBuilderForTest.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setGroupId(1)
                        .build();
        OmniboxSuggestion suggestionForGroup2 =
                OmniboxSuggestionBuilderForTest.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setGroupId(2)
                        .build();

        list.add(suggestionWithNoGroup);
        list.add(suggestionForGroup1);
        list.add(suggestionForGroup1);
        list.add(suggestionForGroup2);
        list.add(suggestionForGroup2);

        mMediator.onSuggestionsReceived(new AutocompleteResult(list, headers), "a");
        final ModelList model = mMediator.getSuggestionModelList();
        Assert.assertEquals(5 /* suggestions */ + 2 /* group headers */, model.size());

        Assert.assertEquals(
                ((DropdownItemViewInfo) model.get(0)).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(
                ((DropdownItemViewInfo) model.get(1)).type, OmniboxSuggestionUiType.HEADER);
        Assert.assertEquals(
                ((DropdownItemViewInfo) model.get(2)).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(
                ((DropdownItemViewInfo) model.get(3)).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(
                ((DropdownItemViewInfo) model.get(4)).type, OmniboxSuggestionUiType.HEADER);
        Assert.assertEquals(
                ((DropdownItemViewInfo) model.get(5)).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(
                ((DropdownItemViewInfo) model.get(6)).type, OmniboxSuggestionUiType.DEFAULT);

        DropdownItemViewInfo header1 = (DropdownItemViewInfo) model.get(1);
        DropdownItemViewInfo header2 = (DropdownItemViewInfo) model.get(4);

        Assert.assertEquals(header1.model.get(HeaderViewProperties.TITLE), headers.get(1));
        Assert.assertEquals(header2.model.get(HeaderViewProperties.TITLE), headers.get(2));
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP})
    public void
    updateSuggestionsList_toggleGroupVisibility() {
        mMediator.onNativeInitialized();
        final List<OmniboxSuggestion> list = Arrays.asList(
                OmniboxSuggestionBuilderForTest.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setDisplayText("Ungrouped 1")
                        .build(),
                OmniboxSuggestionBuilderForTest.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setDisplayText("Ungrouped 2")
                        .build(),

                OmniboxSuggestionBuilderForTest.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setDisplayText("Grouped 1/1")
                        .setGroupId(1)
                        .build(),
                OmniboxSuggestionBuilderForTest.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setDisplayText("Grouped 1/2")
                        .setGroupId(1)
                        .build(),

                OmniboxSuggestionBuilderForTest.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setDisplayText("Grouped 2/1")
                        .setGroupId(2)
                        .build(),
                OmniboxSuggestionBuilderForTest.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setDisplayText("Grouped 2/2")
                        .setGroupId(2)
                        .build());

        final SparseArray<String> headers = buildDummyGroupHeaders(3, 1, "Header1");
        mMediator.onSuggestionsReceived(new AutocompleteResult(list, headers), "a");

        final List<DropdownItemViewInfo> viewInfos = mMediator.getSuggestionViewInfoListForTest();
        final ModelList modelList = mMediator.getSuggestionModelList();

        // Check that initially both lists contain same elements.
        Assert.assertEquals(viewInfos.size(), modelList.size());
        for (int index = 0; index < viewInfos.size(); index++) {
            Assert.assertEquals(viewInfos.get(index), modelList.get(index));
        }

        // Hide items belonging to group 1. Check that only suggestions for group 1 are hidden, and
        // the order is maintained.
        mMediator.setGroupVisibility(1, false);
        // We hid 2 suggestions.
        Assert.assertEquals(viewInfos.size() - 2, modelList.size());
        int modelIndex = 0;
        for (int index = 0; index < viewInfos.size(); index++) {
            DropdownItemViewInfo info = viewInfos.get(index);
            if (info.type != OmniboxSuggestionUiType.HEADER && info.groupId == 1) continue;
            Assert.assertEquals("Wrong item at position " + index, info, modelList.get(modelIndex));
            modelIndex++;
        }

        // Hide items belonging to group 2. Check that suggestion for both groups 1 and 2 are
        // hidden, and the order is maintained.
        mMediator.setGroupVisibility(2, false);
        // We now hid 4 suggestions: 2 for group 1 and 2 for group 2.
        Assert.assertEquals(viewInfos.size() - 4, modelList.size());
        modelIndex = 0;
        for (int index = 0; index < viewInfos.size(); index++) {
            DropdownItemViewInfo info = viewInfos.get(index);
            if (info.type != OmniboxSuggestionUiType.HEADER
                    && (info.groupId == 1 || info.groupId == 2)) {
                continue;
            }
            Assert.assertEquals(info, modelList.get(modelIndex));
            modelIndex++;
        }

        // Show items belonging to group 1. Check that only suggestion for group 2 are hidden.
        mMediator.setGroupVisibility(1, true);
        // We now hide just 2 suggestions belonging to group 2.
        Assert.assertEquals(viewInfos.size() - 2, modelList.size());
        modelIndex = 0;
        for (int index = 0; index < viewInfos.size(); index++) {
            DropdownItemViewInfo info = viewInfos.get(index);
            if (info.type != OmniboxSuggestionUiType.HEADER && info.groupId == 2) continue;
            Assert.assertEquals(info, modelList.get(modelIndex));
            modelIndex++;
        }

        // Show items belonging to group 1. Check that only suggestion for group 2 are hidden.
        mMediator.setGroupVisibility(2, true);
        // Check that in the end both lists contain same elements.
        Assert.assertEquals(viewInfos.size(), modelList.size());
        for (int index = 0; index < viewInfos.size(); index++) {
            Assert.assertEquals(viewInfos.get(index), modelList.get(index));
        }
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP})
    public void
    updateSuggestionsList_expandingAlreadyExpandedGroupAddsNoNewElementns() {
        mMediator.onNativeInitialized();
        final List<OmniboxSuggestion> list = Arrays.asList(
                OmniboxSuggestionBuilderForTest.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setDisplayText("Ungrouped 1")
                        .build(),

                OmniboxSuggestionBuilderForTest.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setDisplayText("Grouped 1/1")
                        .setGroupId(1)
                        .build(),
                OmniboxSuggestionBuilderForTest.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setDisplayText("Grouped 1/2")
                        .setGroupId(1)
                        .build());

        final SparseArray<String> headers = buildDummyGroupHeaders(1, 1, "Header1");
        mMediator.onSuggestionsReceived(new AutocompleteResult(list, headers), "a");

        final List<DropdownItemViewInfo> viewInfos = mMediator.getSuggestionViewInfoListForTest();
        final ModelList modelList = mMediator.getSuggestionModelList();

        // Check that initially both lists contain same elements.
        Assert.assertEquals(viewInfos.size(), modelList.size());
        // Expand already expanded group 1.
        mMediator.setGroupVisibility(1, true);
        // Check that in the end both lists contain same elements.
        Assert.assertEquals(viewInfos.size(), modelList.size());
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP})
    public void
    updateSuggestionsList_collapseAlreadyCollapsedListIsNoOp() {
        mMediator.onNativeInitialized();
        final List<OmniboxSuggestion> list = Arrays.asList(
                OmniboxSuggestionBuilderForTest.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setDisplayText("Ungrouped 1")
                        .build(),

                OmniboxSuggestionBuilderForTest.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setDisplayText("Grouped 1/1")
                        .setGroupId(1)
                        .build(),
                OmniboxSuggestionBuilderForTest.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setDisplayText("Grouped 1/2")
                        .setGroupId(1)
                        .build());

        final SparseArray<String> headers = buildDummyGroupHeaders(1, 1, "Header1");
        mMediator.onSuggestionsReceived(new AutocompleteResult(list, headers), "a");

        // Only the first two elements on the list should be displayed once we collapse group 1:
        // - the ungrouped suggestion
        // - the group header.
        final List<DropdownItemViewInfo> viewInfos =
                mMediator.getSuggestionViewInfoListForTest().subList(0, 2);
        final ModelList modelList = mMediator.getSuggestionModelList();

        // Collapse group 1.
        mMediator.setGroupVisibility(1, false);
        Assert.assertEquals(viewInfos.size(), modelList.size());

        // Collapse the group again.
        mMediator.setGroupVisibility(1, false);
        Assert.assertEquals(viewInfos.size(), modelList.size());
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @NativeJavaTestFeatures.Enable(ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP)
    public void updateSuggestionsList_defersKeyboardPopupWhenHaveLotsOfSuggestionsToShow() {
        mMediator.onNativeInitialized();
        mMediator.signalPendingKeyboardShowDecision();
        mMediator.onSuggestionsReceived(new AutocompleteResult(mSuggestionsList, null), "");
        verify(mAutocompleteDelegate, times(1)).setKeyboardVisibility(eq(false));
        verify(mAutocompleteDelegate, never()).setKeyboardVisibility(eq(true));
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @NativeJavaTestFeatures.Enable(ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP)
    public void updateSuggestionsList_showsKeyboardWhenHaveFewSuggestionsToShow() {
        mMediator.onNativeInitialized();
        mMediator.signalPendingKeyboardShowDecision();
        mMediator.onSuggestionsReceived(
                new AutocompleteResult(
                        mSuggestionsList.subList(0, MINIMUM_NUMBER_OF_SUGGESTIONS_TO_SHOW), null),
                "");
        verify(mAutocompleteDelegate, times(1)).setKeyboardVisibility(eq(true));
        verify(mAutocompleteDelegate, never()).setKeyboardVisibility(eq(false));
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @NativeJavaTestFeatures.Enable(ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP)
    public void updateSuggestionsList_doesNotShowKeyboardAfterReceivingSubsequentSuggestionLists() {
        mMediator.onNativeInitialized();
        mMediator.signalPendingKeyboardShowDecision();
        mMediator.onSuggestionsReceived(new AutocompleteResult(mSuggestionsList, null), "");
        mMediator.onSuggestionsReceived(
                new AutocompleteResult(mSuggestionsList.subList(0, 1), null), "");
        verify(mAutocompleteDelegate, times(1)).setKeyboardVisibility(eq(false));
        verify(mAutocompleteDelegate, never()).setKeyboardVisibility(eq(true));
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @NativeJavaTestFeatures.Enable(ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP)
    public void updateSuggestionsList_hidesKeyboardOnScrollWithLotsOfSuggestions() {
        // It is quite important that we hide keyboard every time the user initiates scroll.
        // The reason for this is that the keyboard could be requested by the user when they press
        // the omnibox field. This is beyond our control.
        mMediator.onNativeInitialized();
        mMediator.signalPendingKeyboardShowDecision();
        mMediator.onSuggestionsReceived(new AutocompleteResult(mSuggestionsList, null), "");
        verify(mAutocompleteDelegate, times(1)).setKeyboardVisibility(eq(false));
        // Should request keyboard hide.
        mMediator.onSuggestionDropdownScroll();
        verify(mAutocompleteDelegate, times(2)).setKeyboardVisibility(eq(false));
        // Should also request keyboard hide.
        mMediator.onSuggestionDropdownScroll();
        verify(mAutocompleteDelegate, times(3)).setKeyboardVisibility(eq(false));
        verify(mAutocompleteDelegate, never()).setKeyboardVisibility(eq(true));
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @NativeJavaTestFeatures.Enable(ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP)
    public void updateSuggestionsList_retainsKeyboardOnScrollWithFewSuggestions() {
        mMediator.onNativeInitialized();
        mMediator.signalPendingKeyboardShowDecision();
        mMediator.onSuggestionsReceived(
                new AutocompleteResult(
                        mSuggestionsList.subList(0, MINIMUM_NUMBER_OF_SUGGESTIONS_TO_SHOW), null),
                "");
        verify(mAutocompleteDelegate, times(1)).setKeyboardVisibility(eq(true));

        // Should perform no action.
        mMediator.onSuggestionDropdownScroll();
        // Should perform no action.
        mMediator.onSuggestionDropdownScroll();

        verify(mAutocompleteDelegate, never()).setKeyboardVisibility(eq(false));
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP})
    public void
    updateSuggestionsList_suggestionsAreRebuiltOnSubsequentInteractions() {
        // This test validates scenario:
        // 1. user focuses omnibox
        // 2. AutocompleteMediator receives suggestions
        // 3. user sees suggestions, but leaves omnibox
        // 4. user focuses omnibox again
        // 5. AutocompleteMediator receives same suggestions as in (2)
        // 6. user sees suggestions again.
        final List<OmniboxSuggestion> res1 = buildDummySuggestionsList(10, "Suggestion");
        final List<OmniboxSuggestion> res2 = buildDummySuggestionsList(10, "Suggestion");
        Assert.assertEquals(res1, res2);

        mMediator.onNativeInitialized();
        mMediator.onSuggestionsReceived(new AutocompleteResult(res1, null), "");
        Assert.assertEquals(mMediator.getSuggestionViewInfoListForTest().size(), res1.size());
        Assert.assertTrue(mListModel.get(SuggestionListProperties.VISIBLE));

        mMediator.onUrlFocusChange(false);
        Assert.assertFalse(mListModel.get(SuggestionListProperties.VISIBLE));
        Assert.assertEquals(mMediator.getSuggestionViewInfoListForTest().size(), 0);

        // Simulate omnibox focused. this bypasses the native call to fetch suggestions.
        mMediator.setSuggestionVisibilityState(
                AutocompleteMediator.SuggestionVisibilityState.ALLOWED);

        mMediator.onSuggestionsReceived(new AutocompleteResult(res2, null), "");
        Assert.assertEquals(mMediator.getSuggestionViewInfoListForTest().size(), res2.size());
        Assert.assertTrue(mListModel.get(SuggestionListProperties.VISIBLE));
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
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

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
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
        verify(mAutocompleteController)
                .start(profile, url, pageClassification, "test", 4, false, null);
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
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
        verify(mAutocompleteController)
                .start(profile, url, pageClassification, "nottest", 4, false, null);
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP})
    public void onSuggestionsReceived_sendsOnSuggestionsChanged() {
        mMediator.onNativeInitialized();
        mMediator.onSuggestionsReceived(
                new AutocompleteResult(mSuggestionsList, null), "inline_autocomplete");
        verify(mAutocompleteDelegate).onSuggestionsChanged("inline_autocomplete");

        // Ensure duplicate requests are suppressed.
        mMediator.onSuggestionsReceived(
                new AutocompleteResult(mSuggestionsList, null), "inline_autocomplete2");
        verifyNoMoreInteractions(mAutocompleteDelegate);
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP})
    public void setLayoutDirection_beforeInitialization() {
        mMediator.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        mMediator.setNewSuggestions(new AutocompleteResult(mSuggestionsList, null));
        mMediator.updateSuggestionsList(Integer.MAX_VALUE);
        Assert.assertEquals(mSuggestionsList.size(), mSuggestionModels.size());
        for (int i = 0; i < mSuggestionModels.size(); i++) {
            Assert.assertEquals(i + "th model does not have the expected layout direction.",
                    View.LAYOUT_DIRECTION_RTL,
                    mSuggestionModels.get(i).model.get(
                            SuggestionCommonProperties.LAYOUT_DIRECTION));
        }
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP})
    public void setLayoutDirection_afterInitialization() {
        mMediator.setNewSuggestions(new AutocompleteResult(mSuggestionsList, null));
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

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
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

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
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
        verify(mAutocompleteController)
                .startZeroSuggest(profile, "", url, pageClassification, title);
    }
}
