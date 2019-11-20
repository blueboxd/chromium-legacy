// Copyright 2018 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.android.libraries.feed.piet.testing;

import static com.google.android.libraries.feed.common.Validators.checkNotNull;
import static com.google.android.libraries.feed.common.Validators.checkState;

import android.content.Context;
import android.view.View;
import android.widget.LinearLayout;
import com.google.android.libraries.feed.piet.FrameAdapter;
import com.google.android.libraries.feed.piet.host.ActionHandler;
import com.google.android.libraries.feed.piet.host.ActionHandler.ActionType;
import com.google.search.now.ui.piet.ActionsProto.Action;
import com.google.search.now.ui.piet.LogDataProto.LogData;
import com.google.search.now.ui.piet.PietAndroidSupport;
import com.google.search.now.ui.piet.PietProto.Frame;
import com.google.search.now.ui.piet.PietProto.PietSharedState;
import java.util.ArrayList;
import java.util.List;

/** Fake for {@link FrameAdapter}. */
public class FakeFrameAdapter implements FrameAdapter {

  private final List<Action> viewActions;
  private final List<Action> hideActions;
  private final LinearLayout frameContainer;
  private final ActionHandler actionHandler;

  public boolean isBound;
  private boolean viewActionsTriggered;
  /*@Nullable*/ private Frame frame;

  private FakeFrameAdapter(
      Context context,
      List<Action> viewActions,
      List<Action> hideActions,
      ActionHandler actionHandler) {
    this.viewActions = viewActions;
    this.frameContainer = new LinearLayout(context);
    this.actionHandler = actionHandler;
    this.hideActions = hideActions;
  }

  @Override
  public void bindModel(
      Frame frame,
      int frameWidthPx,
      PietAndroidSupport./*@Nullable*/ ShardingControl shardingControl,
      List<PietSharedState> pietSharedStates) {
    checkState(!isBound);
    this.frame = frame;
    isBound = true;
  }

  @Override
  public void unbindModel() {
    checkState(isBound);
    triggerHideActions();
    frame = null;
    isBound = false;
    viewActionsTriggered = false;
  }

  @Override
  public LinearLayout getFrameContainer() {
    return frameContainer;
  }

  @Override
  public void triggerHideActions() {
    if (!viewActionsTriggered) {
      return;
    }

    if (!isBound) {
      return;
    }

    for (Action action : hideActions) {
      actionHandler.handleAction(
          action,
          ActionType.VIEW,
          checkNotNull(frame),
          frameContainer,
          LogData.getDefaultInstance());
    }

    viewActionsTriggered = false;
  }

  @Override
  public void triggerViewActions(View viewport) {
    if (viewActionsTriggered) {
      return;
    }

    if (!isBound) {
      return;
    }

    // Assume all view actions are fired when triggerViewActions is called.
    for (Action action : viewActions) {
      actionHandler.handleAction(
          action,
          ActionType.VIEW,
          checkNotNull(frame),
          frameContainer,
          /* logData= */ LogData.getDefaultInstance());
    }

    viewActionsTriggered = true;
  }

  /** Returns whether the {@link FrameAdapter} is bound. */
  public boolean isBound() {
    return isBound;
  }

  /** Returns a new builder for a {@link FakeFrameAdapter} */
  public static Builder builder(Context context) {
    return new Builder(context);
  }

  /** Builder for {@link FakeFrameAdapter}. */
  public static final class Builder {

    private final Context context;
    private final List<Action> viewActions;
    private final List<Action> hideActions;

    private ActionHandler actionHandler;

    private Builder(Context context) {
      this.context = context;
      viewActions = new ArrayList<>();
      hideActions = new ArrayList<>();
      actionHandler = new NoOpActionHandler();
    }

    public Builder addViewAction(Action viewAction) {
      viewActions.add(viewAction);
      return this;
    }

    public Builder addHideAction(Action hideAction) {
      hideActions.add(hideAction);
      return this;
    }

    /**
     * Sets the {@link ActionHandler} to be used when triggering actions. If not set, actions will
     * no-op.
     */
    public Builder setActionHandler(ActionHandler actionHandler) {
      this.actionHandler = actionHandler;
      return this;
    }

    public FakeFrameAdapter build() {
      return new FakeFrameAdapter(context, viewActions, hideActions, actionHandler);
    }
  }

  private static final class NoOpActionHandler implements ActionHandler {

    @Override
    public void handleAction(
        Action action, @ActionType int actionType, Frame frame, View view, LogData logData) {}
  }
}
