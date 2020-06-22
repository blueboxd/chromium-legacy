// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.util.SparseArray;

import org.junit.Assert;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.CalledByNativeJavaTest;
import org.chromium.base.annotations.NativeJavaTestFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderProcessor;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link DropdownItemViewInfoListBuilder}.
 */
public class DropdownItemViewInfoListBuilderUnitTest {
    @Mock
    SuggestionProcessor mMockSuggestionProcessor;

    @Mock
    HeaderProcessor mMockHeaderProcessor;

    DropdownItemViewInfoListBuilder mBuilder;

    @CalledByNative
    private DropdownItemViewInfoListBuilderUnitTest() {}

    @CalledByNative
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        when(mMockSuggestionProcessor.doesProcessSuggestion(any(), anyInt())).thenReturn(true);
        when(mMockSuggestionProcessor.createModel())
                .thenAnswer((mock) -> new PropertyModel(SuggestionCommonProperties.ALL_KEYS));
        when(mMockSuggestionProcessor.getViewTypeId()).thenReturn(OmniboxSuggestionUiType.DEFAULT);

        when(mMockHeaderProcessor.createModel())
                .thenAnswer((mock) -> new PropertyModel(SuggestionCommonProperties.ALL_KEYS));
        when(mMockHeaderProcessor.getViewTypeId()).thenReturn(OmniboxSuggestionUiType.HEADER);

        mBuilder = new DropdownItemViewInfoListBuilder();
        mBuilder.registerSuggestionProcessor(mMockSuggestionProcessor);
        mBuilder.setHeaderProcessorForTest(mMockHeaderProcessor);
    }

    /**
     * Verify that two lists have exactly same content.
     * Note: this works similarly to Assert.assertEquals(list1, list2), but instead of printing out
     * the content of both lists, simply reports elements that differ.
     * OmniboxSuggestion.toString() is verbose enough that the result analysis may be difficult or
     * even impossible for a small list if the output exceeds the Android's logcat entry length
     * limit.
     */
    private <T> void verifyListsMatch(List<T> expected, List<T> actual) {
        Assert.assertEquals(expected.size(), actual.size());
        for (int index = 0; index < expected.size(); index++) {
            Assert.assertEquals("Item at position " + index + " does not match",
                    expected.get(index), actual.get(index));
        }
    }

    @CalledByNativeJavaTest
    public void headers_buildsHeaderForFirstSuggestion() {
        final List<OmniboxSuggestion> actualList = new ArrayList<>();
        final SparseArray<String> headers = new SparseArray<>();
        headers.put(1, "Header 1");

        OmniboxSuggestion suggestion =
                OmniboxSuggestionBuilderForTest.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setGroupId(1)
                        .build();

        actualList.add(suggestion);
        actualList.add(suggestion);

        final InOrder verifier = inOrder(mMockSuggestionProcessor, mMockHeaderProcessor);
        final List<DropdownItemViewInfo> model =
                mBuilder.buildDropdownViewInfoList(new AutocompleteResult(actualList, headers));

        verifier.verify(mMockHeaderProcessor, times(1)).populateModel(any(), eq(1), eq("Header 1"));
        verifier.verify(mMockSuggestionProcessor, times(1))
                .populateModel(eq(suggestion), any(), eq(0));
        verifier.verify(mMockSuggestionProcessor, times(1))
                .populateModel(eq(suggestion), any(), eq(1));
        Assert.assertEquals(3, model.size()); // 1 header + 2 suggestions.

        Assert.assertEquals(model.get(0).type, OmniboxSuggestionUiType.HEADER);
        Assert.assertEquals(model.get(0).processor, mMockHeaderProcessor);
        Assert.assertEquals(model.get(0).groupId, 1);
        Assert.assertEquals(model.get(1).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(model.get(1).processor, mMockSuggestionProcessor);
        Assert.assertEquals(model.get(1).groupId, 1);
        Assert.assertEquals(model.get(2).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(model.get(2).processor, mMockSuggestionProcessor);
        Assert.assertEquals(model.get(2).groupId, 1);
    }

    @CalledByNativeJavaTest
    public void headers_buildsHeadersOnlyWhenGroupChanges() {
        final List<OmniboxSuggestion> actualList = new ArrayList<>();
        final SparseArray<String> headers = new SparseArray<>();
        headers.put(1, "Header 1");
        headers.put(2, "Header 2");

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

        actualList.add(suggestionWithNoGroup);
        actualList.add(suggestionForGroup1);
        actualList.add(suggestionForGroup1);
        actualList.add(suggestionForGroup2);
        actualList.add(suggestionForGroup2);

        final InOrder verifier = inOrder(mMockSuggestionProcessor, mMockHeaderProcessor);
        final List<DropdownItemViewInfo> model =
                mBuilder.buildDropdownViewInfoList(new AutocompleteResult(actualList, headers));

        verifier.verify(mMockSuggestionProcessor, times(1))
                .populateModel(eq(suggestionWithNoGroup), any(), eq(0));
        verifier.verify(mMockHeaderProcessor, times(1)).populateModel(any(), eq(1), eq("Header 1"));
        verifier.verify(mMockSuggestionProcessor, times(1))
                .populateModel(eq(suggestionForGroup1), any(), eq(1));
        verifier.verify(mMockSuggestionProcessor, times(1))
                .populateModel(eq(suggestionForGroup1), any(), eq(2));
        verifier.verify(mMockHeaderProcessor, times(1)).populateModel(any(), eq(2), eq("Header 2"));
        verifier.verify(mMockSuggestionProcessor, times(1))
                .populateModel(eq(suggestionForGroup2), any(), eq(3));
        verifier.verify(mMockSuggestionProcessor, times(1))
                .populateModel(eq(suggestionForGroup2), any(), eq(4));
        Assert.assertEquals(7, model.size()); // 2 headers + 5 suggestions.

        Assert.assertEquals(model.get(0).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(model.get(0).groupId, -1);

        Assert.assertEquals(model.get(1).type, OmniboxSuggestionUiType.HEADER);
        Assert.assertEquals(model.get(1).groupId, 1);
        Assert.assertEquals(model.get(2).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(model.get(2).groupId, 1);
        Assert.assertEquals(model.get(3).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(model.get(3).groupId, 1);

        Assert.assertEquals(model.get(4).type, OmniboxSuggestionUiType.HEADER);
        Assert.assertEquals(model.get(4).groupId, 2);
        Assert.assertEquals(model.get(5).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(model.get(5).groupId, 2);
        Assert.assertEquals(model.get(6).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(model.get(6).groupId, 2);
    }

    @CalledByNativeJavaTest
    public void builder_propagatesFocusChangeEvents() {
        mBuilder.onUrlFocusChange(true);
        verify(mMockHeaderProcessor, times(1)).onUrlFocusChange(eq(true));
        verify(mMockSuggestionProcessor, times(1)).onUrlFocusChange(eq(true));

        mBuilder.onUrlFocusChange(false);
        verify(mMockHeaderProcessor, times(1)).onUrlFocusChange(eq(false));
        verify(mMockSuggestionProcessor, times(1)).onUrlFocusChange(eq(false));

        verifyNoMoreInteractions(mMockHeaderProcessor);
        verifyNoMoreInteractions(mMockSuggestionProcessor);
    }

    @NativeJavaTestFeatures.Disable(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @CalledByNativeJavaTest
    public void builder_propagatesNativeInitializedEvent() {
        mBuilder.onNativeInitialized();
        verify(mMockHeaderProcessor, times(1)).onNativeInitialized();
        verify(mMockSuggestionProcessor, times(1)).onNativeInitialized();

        verifyNoMoreInteractions(mMockHeaderProcessor);
        verifyNoMoreInteractions(mMockSuggestionProcessor);
    }

    @NativeJavaTestFeatures.Enable(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @CalledByNativeJavaTest
    public void grouping_noGroupingForSuggestionsWithHeaders() {
        mBuilder.onNativeInitialized();
        final List<OmniboxSuggestion> actualList = new ArrayList<>();
        final SparseArray<String> headers = new SparseArray<>();
        headers.put(1, "Header 1");

        OmniboxSuggestionBuilderForTest builder =
                OmniboxSuggestionBuilderForTest.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setGroupId(1);

        // Build 4 mixed search/url suggestions with headers.
        actualList.add(builder.setIsSearch(true).build());
        actualList.add(builder.setIsSearch(false).build());
        actualList.add(builder.setIsSearch(true).build());
        actualList.add(builder.setIsSearch(false).build());

        final List<OmniboxSuggestion> expectedList = new ArrayList<>();
        expectedList.addAll(actualList);

        mBuilder.groupSuggestionsBySearchVsURL(actualList, 4);
        Assert.assertEquals(actualList, expectedList);
    }

    @NativeJavaTestFeatures.Enable(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @CalledByNativeJavaTest
    public void grouping_shortMixedContentGrouping() {
        mBuilder.onNativeInitialized();
        final List<OmniboxSuggestion> actualList = new ArrayList<>();

        OmniboxSuggestionBuilderForTest builder = OmniboxSuggestionBuilderForTest.searchWithType(
                OmniboxSuggestionType.SEARCH_SUGGEST);

        // Default match.
        actualList.add(builder.setRelevance(0).setIsSearch(false).build());
        // Build 4 mixed search/url suggestions.
        actualList.add(builder.setRelevance(16).setIsSearch(false).build());
        actualList.add(builder.setRelevance(14).setIsSearch(true).build());
        actualList.add(builder.setRelevance(12).setIsSearch(false).build());
        actualList.add(builder.setRelevance(10).setIsSearch(true).build());

        // Build 4 mixed search/url suggestions with headers.
        builder.setGroupId(1);
        actualList.add(builder.setIsSearch(true).build());
        actualList.add(builder.setIsSearch(false).build());
        actualList.add(builder.setIsSearch(true).build());
        actualList.add(builder.setIsSearch(false).build());

        final List<OmniboxSuggestion> expectedList = new ArrayList<>();
        expectedList.add(actualList.get(0)); // Default match.
        expectedList.add(actualList.get(2)); // Highest scored search suggestion
        expectedList.add(actualList.get(4)); // Next highest scored search suggestion
        expectedList.add(actualList.get(1)); // Highest scored url suggestion
        expectedList.add(actualList.get(3)); // Next highest scored url suggestion
        expectedList.addAll(actualList.subList(5, 9));

        mBuilder.groupSuggestionsBySearchVsURL(actualList, 8);
        verifyListsMatch(expectedList, actualList);
    }

    @NativeJavaTestFeatures.Enable(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @CalledByNativeJavaTest
    public void grouping_longMixedContentGrouping() {
        mBuilder.onNativeInitialized();
        final List<OmniboxSuggestion> actualList = new ArrayList<>();

        OmniboxSuggestionBuilderForTest builder = OmniboxSuggestionBuilderForTest.searchWithType(
                OmniboxSuggestionType.SEARCH_SUGGEST);

        // Default match.
        actualList.add(builder.setRelevance(0).setIsSearch(false).build());
        // Build 6 mixed search/url suggestions.
        actualList.add(builder.setRelevance(18).setIsSearch(true).build());
        actualList.add(builder.setRelevance(16).setIsSearch(false).build());
        actualList.add(builder.setRelevance(14).setIsSearch(true).build());
        actualList.add(builder.setRelevance(12).setIsSearch(false).build());
        actualList.add(builder.setRelevance(10).setIsSearch(true).build());
        actualList.add(builder.setRelevance(8).setIsSearch(false).build());

        // Build 4 mixed search/url suggestions with headers.
        builder.setGroupId(1);
        actualList.add(builder.setRelevance(100).setIsSearch(true).build());
        actualList.add(builder.setRelevance(100).setIsSearch(false).build());

        // Request splitting point to be at 4 suggestions.
        // This should split suggestions into 2 groups:
        // - relevance 18, 14, 16 (in this order)
        // - relevance 10, 18, 8 and 100 (in this order)
        final List<OmniboxSuggestion> expectedList = new ArrayList<>();

        // 3 visible suggestions
        expectedList.add(actualList.get(0)); // Default match.
        expectedList.add(actualList.get(1)); // Search suggestion scored 18
        expectedList.add(actualList.get(2)); // URL suggestion scored 16

        // Remaining, invisible suggestions
        expectedList.add(actualList.get(3)); // Search suggestion scored 14
        expectedList.add(actualList.get(5)); // Search suggestion scored 10
        expectedList.add(actualList.get(4)); // URL suggestion scored 12
        expectedList.add(actualList.get(6)); // URL suggestion scored 8
        expectedList.addAll(actualList.subList(7, 9)); // Grouped suggestions.

        mBuilder.groupSuggestionsBySearchVsURL(actualList, 3);
        verifyListsMatch(expectedList, actualList);
    }

    @NativeJavaTestFeatures.Enable(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @CalledByNativeJavaTest
    public void grouping_longHeaderlessContentGrouping() {
        mBuilder.onNativeInitialized();
        final List<OmniboxSuggestion> actualList = new ArrayList<>();

        OmniboxSuggestionBuilderForTest builder = OmniboxSuggestionBuilderForTest.searchWithType(
                OmniboxSuggestionType.SEARCH_SUGGEST);

        // Default match.
        actualList.add(builder.setRelevance(0).setIsSearch(false).build());
        // Build 8 mixed search/url suggestions.
        // The order is intentionally descending, as SortAndCull would order these items like this
        // for us.
        actualList.add(builder.setRelevance(20).setIsSearch(false).build());
        actualList.add(builder.setRelevance(18).setIsSearch(true).build());
        actualList.add(builder.setRelevance(16).setIsSearch(false).build());
        actualList.add(builder.setRelevance(14).setIsSearch(true).build());
        actualList.add(builder.setRelevance(12).setIsSearch(false).build());
        actualList.add(builder.setRelevance(10).setIsSearch(true).build());
        actualList.add(builder.setRelevance(8).setIsSearch(false).build());
        actualList.add(builder.setRelevance(6).setIsSearch(false).build());

        // Request splitting point to be at 4 suggestions.
        // This should split suggestions into 2 groups:
        // - relevance 18, 14, 20, 16 (in this order)
        // - relevance 10, 18, 8 and 100 (in this order)
        final List<OmniboxSuggestion> expectedList = Arrays.asList(
                // Top 4, visible suggestions
                actualList.get(0), // Default match
                actualList.get(2), // Search suggestion scored 18
                actualList.get(1), // URL suggestion scored 20
                actualList.get(3), // URL suggestion scored 16

                actualList.get(4), // Search suggestion scored 14
                actualList.get(6), // Search suggestion scored 10
                actualList.get(5), // URL suggestion scored 12
                actualList.get(7), // URL suggestion scored 8
                actualList.get(8)); // URL suggestion scored 6

        mBuilder.groupSuggestionsBySearchVsURL(actualList, 4);
        verifyListsMatch(expectedList, actualList);
    }
}
