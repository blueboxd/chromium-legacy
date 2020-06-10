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
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderProcessor;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
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
     * Build a fake suggestions list with elements named 'Suggestion #', where '#' is the suggestion
     * index (1-based).
     *
     * @return List of suggestions.
     */
    private List<OmniboxSuggestion> buildDummySuggestionsList(int count, String prefix) {
        List<OmniboxSuggestion> list = new ArrayList<>();
        for (int index = 0; index < count; index++) {
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
    public void headers_buildsHeaderForFirstSuggestion() {
        final List<OmniboxSuggestion> list = new ArrayList<>();
        final SparseArray<String> headers = buildDummyGroupHeaders(1, 1, "Header");

        OmniboxSuggestion suggestion =
                OmniboxSuggestionBuilderForTest.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setGroupId(1)
                        .build();

        list.add(suggestion);
        list.add(suggestion);

        final InOrder verifier = inOrder(mMockSuggestionProcessor, mMockHeaderProcessor);
        final List<DropdownItemViewInfo> model =
                mBuilder.buildDropdownViewInfoList(new AutocompleteResult(list, headers));

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

        final InOrder verifier = inOrder(mMockSuggestionProcessor, mMockHeaderProcessor);
        final List<DropdownItemViewInfo> model =
                mBuilder.buildDropdownViewInfoList(new AutocompleteResult(list, headers));

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

    @CalledByNativeJavaTest
    public void builder_propagatesNativeInitializedEvent() {
        mBuilder.onNativeInitialized();
        verify(mMockHeaderProcessor, times(1)).onNativeInitialized();
        verify(mMockSuggestionProcessor, times(1)).onNativeInitialized();

        verifyNoMoreInteractions(mMockHeaderProcessor);
        verifyNoMoreInteractions(mMockSuggestionProcessor);
    }
}
