// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.testing;

import static org.chromium.chrome.browser.feed.library.common.Validators.checkNotNull;
import static org.chromium.chrome.browser.feed.library.common.Validators.checkState;

import android.content.Context;
import android.view.View;
import android.widget.LinearLayout;

import org.chromium.chrome.browser.feed.library.piet.FrameAdapter;
import org.chromium.chrome.browser.feed.library.piet.host.ActionHandler;
import org.chromium.chrome.browser.feed.library.piet.host.ActionHandler.ActionType;
import org.chromium.components.feed.core.proto.ui.piet.ActionsProto.Action;
import org.chromium.components.feed.core.proto.ui.piet.LogDataProto.LogData;
import org.chromium.components.feed.core.proto.ui.piet.PietAndroidSupport;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Frame;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.PietSharedState;

import java.util.ArrayList;
import java.util.List;

/** Fake for {@link FrameAdapter}. */
public class FakeFrameAdapter implements FrameAdapter {
    private final List<Action> mViewActions;
    private final List<Action> mHideActions;
    private final LinearLayout mFrameContainer;
    private final ActionHandler mActionHandler;

    public boolean mIsBound;
    private boolean mViewActionsTriggered;
    /*@Nullable*/ private Frame mFrame;

    private FakeFrameAdapter(Context context, List<Action> viewActions, List<Action> hideActions,
            ActionHandler actionHandler) {
        this.mViewActions = viewActions;
        this.mFrameContainer = new LinearLayout(context);
        this.mActionHandler = actionHandler;
        this.mHideActions = hideActions;
    }

    @Override
    public void bindModel(Frame frame, int frameWidthPx,
            PietAndroidSupport./*@Nullable*/ ShardingControl shardingControl,
            List<PietSharedState> pietSharedStates) {
        checkState(!mIsBound);
        this.mFrame = frame;
        mIsBound = true;
    }

    @Override
    public void unbindModel() {
        checkState(mIsBound);
        triggerHideActions();
        mFrame = null;
        mIsBound = false;
        mViewActionsTriggered = false;
    }

    @Override
    public LinearLayout getFrameContainer() {
        return mFrameContainer;
    }

    @Override
    public void triggerHideActions() {
        if (!mViewActionsTriggered) {
            return;
        }

        if (!mIsBound) {
            return;
        }

        for (Action action : mHideActions) {
            mActionHandler.handleAction(action, ActionType.VIEW, checkNotNull(mFrame),
                    mFrameContainer, LogData.getDefaultInstance());
        }

        mViewActionsTriggered = false;
    }

    @Override
    public void triggerViewActions(View viewport) {
        if (mViewActionsTriggered) {
            return;
        }

        if (!mIsBound) {
            return;
        }

        // Assume all view actions are fired when triggerViewActions is called.
        for (Action action : mViewActions) {
            mActionHandler.handleAction(action, ActionType.VIEW, checkNotNull(mFrame),
                    mFrameContainer,
                    /* logData= */ LogData.getDefaultInstance());
        }

        mViewActionsTriggered = true;
    }

    /** Returns whether the {@link FrameAdapter} is bound. */
    public boolean isBound() {
        return mIsBound;
    }

    /** Returns a new builder for a {@link FakeFrameAdapter} */
    public static Builder builder(Context context) {
        return new Builder(context);
    }

    /** Builder for {@link FakeFrameAdapter}. */
    public static final class Builder {
        private final Context mContext;
        private final List<Action> mViewActions;
        private final List<Action> mHideActions;

        private ActionHandler mActionHandler;

        private Builder(Context context) {
            this.mContext = context;
            mViewActions = new ArrayList<>();
            mHideActions = new ArrayList<>();
            mActionHandler = new NoOpActionHandler();
        }

        public Builder addViewAction(Action viewAction) {
            mViewActions.add(viewAction);
            return this;
        }

        public Builder addHideAction(Action hideAction) {
            mHideActions.add(hideAction);
            return this;
        }

        /**
         * Sets the {@link ActionHandler} to be used when triggering actions. If not set, actions
         * will no-op.
         */
        public Builder setActionHandler(ActionHandler actionHandler) {
            this.mActionHandler = actionHandler;
            return this;
        }

        public FakeFrameAdapter build() {
            return new FakeFrameAdapter(mContext, mViewActions, mHideActions, mActionHandler);
        }
    }

    private static final class NoOpActionHandler implements ActionHandler {
        @Override
        public void handleAction(Action action, @ActionType int actionType, Frame frame, View view,
                LogData logData) {}
    }
}
