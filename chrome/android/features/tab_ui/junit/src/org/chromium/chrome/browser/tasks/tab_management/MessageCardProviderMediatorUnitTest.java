// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Unit tests for {@link MessageCardProviderMediator}.
 */
@RunWith(LocalRobolectricTestRunner.class)
public class MessageCardProviderMediatorUnitTest {
    private static final int SUGGESTED_TAB_COUNT = 2;

    private MessageCardProviderMediator mMediator;

    @Mock
    private MessageCardView.DismissActionProvider mUiDismissActionProvider;

    @Mock
    private Context mContext;

    @Mock
    private TabSuggestionMessageService.TabSuggestionMessageData mTabSuggestionMessageData;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        doNothing().when(mUiDismissActionProvider).dismiss(anyInt());
        mMediator = new MessageCardProviderMediator(mContext, mUiDismissActionProvider);
    }

    private void enqueueMessageItem(@MessageService.MessageType int type) {
        switch (type) {
            case MessageService.MessageType.TAB_SUGGESTION:
                when(mTabSuggestionMessageData.getSize()).thenReturn(SUGGESTED_TAB_COUNT);
                when(mTabSuggestionMessageData.getDismissActionProvider())
                        .thenReturn((messageType) -> {});
                when(mTabSuggestionMessageData.getReviewActionProvider()).thenReturn(() -> {});
                mMediator.messageReady(type, mTabSuggestionMessageData);
                break;
            default:
                mMediator.messageReady(type, new MessageService.MessageData() {});
        }
    }

    @Test
    public void getMessageItemsTest() {
        enqueueMessageItem(MessageService.MessageType.TAB_SUGGESTION);

        Assert.assertEquals(1, mMediator.getMessageItems().size());
        Assert.assertTrue(mMediator.getReadyMessageItemsForTesting().isEmpty());
        Assert.assertFalse(mMediator.getShownMessageItemsForTesting().isEmpty());
    }

    @Test
    public void getMessageItemsTest_TwoDifferentTypeMessage() {
        enqueueMessageItem(MessageService.MessageType.TAB_SUGGESTION);

        Assert.assertEquals(1, mMediator.getMessageItems().size());
        Assert.assertTrue(mMediator.getReadyMessageItemsForTesting().isEmpty());
        Assert.assertFalse(mMediator.getShownMessageItemsForTesting().isEmpty());

        enqueueMessageItem(MessageService.MessageType.FOR_TESTING);

        Assert.assertEquals(2, mMediator.getMessageItems().size());
        Assert.assertTrue(mMediator.getReadyMessageItemsForTesting().isEmpty());
        Assert.assertFalse(mMediator.getShownMessageItemsForTesting().isEmpty());
    }

    @Test
    public void invalidate_queuedMessage() {
        enqueueMessageItem(MessageService.MessageType.TAB_SUGGESTION);

        mMediator.messageInvalidate(MessageService.MessageType.TAB_SUGGESTION);

        Assert.assertFalse(mMediator.getReadyMessageItemsForTesting().containsKey(
                MessageService.MessageType.TAB_SUGGESTION));
        Assert.assertFalse(mMediator.getShownMessageItemsForTesting().containsKey(
                MessageService.MessageType.TAB_SUGGESTION));
    }

    @Test
    public void invalidate_shownMessage() {
        enqueueMessageItem(MessageService.MessageType.TAB_SUGGESTION);

        mMediator.getMessageItems();
        mMediator.messageInvalidate(MessageService.MessageType.TAB_SUGGESTION);

        verify(mUiDismissActionProvider).dismiss(anyInt());
        Assert.assertFalse(mMediator.getShownMessageItemsForTesting().containsKey(
                MessageService.MessageType.TAB_SUGGESTION));
        Assert.assertFalse(mMediator.getReadyMessageItemsForTesting().containsKey(
                MessageService.MessageType.TAB_SUGGESTION));
    }

    @Test
    public void buildModel_ForTabSuggestion() {
        enqueueMessageItem(MessageService.MessageType.TAB_SUGGESTION);

        PropertyModel model = mMediator.getReadyMessageItemsForTesting()
                                      .get(MessageService.MessageType.TAB_SUGGESTION)
                                      .model;
        Assert.assertEquals(MessageService.MessageType.TAB_SUGGESTION,
                model.get(MessageCardViewProperties.MESSAGE_TYPE));
    }
}
