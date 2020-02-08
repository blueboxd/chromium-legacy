// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType.IPH;
import static org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType.TAB_SUGGESTION;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * This is a {@link MessageService.MessageObserver} that creates and owns different
 * {@link PropertyModel} based on the message type.
 */
public class MessageCardProviderMediator implements MessageService.MessageObserver {
    /**
     * A class represents a Message.
     */
    public class Message {
        public final @MessageService.MessageType int type;
        public final PropertyModel model;

        Message(int type, PropertyModel model) {
            this.type = type;
            this.model = model;
        }
    }

    private final Context mContext;
    private Map<Integer, Message> mMessageItems = new HashMap<>();
    private Map<Integer, Message> mShownMessageItems = new HashMap<>();
    private MessageCardView.DismissActionProvider mUiDismissActionProvider;

    public MessageCardProviderMediator(
            Context context, MessageCardView.DismissActionProvider uiDismissActionProvider) {
        mContext = context;
        mUiDismissActionProvider = uiDismissActionProvider;
    }
    /**
     * @return A list of {@link Message} that can be shown.
     */
    public List<Message> getMessageItems() {
        mShownMessageItems.putAll(mMessageItems);
        mMessageItems.clear();
        return new ArrayList<>(mShownMessageItems.values());
    }

    private PropertyModel buildModel(int messageType, MessageService.MessageData data) {
        switch (messageType) {
            case TAB_SUGGESTION:
                assert data instanceof TabSuggestionMessageService.TabSuggestionMessageData;
                return TabSuggestionMessageCardViewModel.create(mContext, this::messageInvalidate,
                        (TabSuggestionMessageService.TabSuggestionMessageData) data);
            case IPH:
                assert data instanceof IphMessageService.IphMessageData;
                return IphMessageCardViewModel.create(
                        mContext, this::messageInvalidate, (IphMessageService.IphMessageData) data);
            default:
                return new PropertyModel();
        }
    }

    // MessageObserver implementations.
    @Override
    public void messageReady(
            @MessageService.MessageType int type, MessageService.MessageData data) {
        assert !mShownMessageItems.containsKey(type);

        PropertyModel model = buildModel(type, data);
        mMessageItems.put(type, new Message(type, model));
    }

    @Override
    public void messageInvalidate(@MessageService.MessageType int type) {
        if (mMessageItems.containsKey(type)) {
            mMessageItems.remove(type);
        } else if (mShownMessageItems.containsKey(type)) {
            // run ui dismiss handler;
            mUiDismissActionProvider.dismiss(type);
            mShownMessageItems.remove(type);
        }
    }

    @VisibleForTesting
    Map<Integer, Message> getReadyMessageItemsForTesting() {
        return mMessageItems;
    }

    @VisibleForTesting
    Map<Integer, Message> getShownMessageItemsForTesting() {
        return mShownMessageItems;
    }
}