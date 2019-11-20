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

package com.google.android.libraries.feed.piet;

import static com.google.android.libraries.feed.common.Validators.checkNotNull;
import static com.google.search.now.ui.piet.ErrorsProto.ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT;
import static com.google.search.now.ui.piet.ErrorsProto.ErrorCode.ERR_POOR_FRAME_RATE;

import android.content.Context;
import android.support.annotation.VisibleForTesting;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.LinearLayout;
import com.google.android.libraries.feed.api.host.config.DebugBehavior;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.piet.DebugLogger.MessageType;
import com.google.android.libraries.feed.piet.TemplateBinder.TemplateAdapterModel;
import com.google.android.libraries.feed.piet.host.ActionHandler;
import com.google.android.libraries.feed.piet.host.EventLogger;
import com.google.android.libraries.feed.piet.host.LogDataCallback;
import com.google.search.now.ui.piet.ActionsProto.VisibilityAction;
import com.google.search.now.ui.piet.ElementsProto.BindingContext;
import com.google.search.now.ui.piet.ElementsProto.Content;
import com.google.search.now.ui.piet.ElementsProto.Element;
import com.google.search.now.ui.piet.ElementsProto.TemplateInvocation;
import com.google.search.now.ui.piet.PietAndroidSupport.ShardingControl;
import com.google.search.now.ui.piet.PietProto.Frame;
import com.google.search.now.ui.piet.PietProto.PietSharedState;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * An adapter which manages {@link Frame} instances. Frames will contain one or more slices. This
 * class has additional public methods to support host access to the primary view of the frame
 * before the model is bound to the frame. A frame is basically a vertical LinearLayout of slice
 * Views which are created by {@link ElementAdapter}. This Adapter is not created through a Factory
 * and is managed by the host.
 */
public class FrameAdapterImpl implements FrameAdapter {
  private static final String TAG = "FrameAdapter";

  private static final String GENERIC_EXCEPTION = "Top Level Exception was caught - see logcat";

  private final Set<ElementAdapter<?, ?>> childAdapters;

  private final Context context;
  private final AdapterParameters parameters;
  private final ActionHandler actionHandler;
  private final EventLogger eventLogger;
  private final DebugBehavior debugBehavior;
  private final Set<VisibilityAction> activeActions = new HashSet<>();
  /*@Nullable*/ private LinearLayout view = null;
  /*@Nullable*/ private FrameContext frameContext = null;

  FrameAdapterImpl(
      Context context,
      AdapterParameters parameters,
      ActionHandler actionHandler,
      EventLogger eventLogger,
      DebugBehavior debugBehavior) {
    this.context = context;
    this.parameters = parameters;
    this.actionHandler = actionHandler;
    this.eventLogger = eventLogger;
    this.debugBehavior = debugBehavior;
    childAdapters = new HashSet<>();
  }

  // TODO: Need to implement support for sharding
  @Override
  public void bindModel(
      Frame frame,
      int frameWidthPx,
      /*@Nullable*/ ShardingControl shardingControl,
      List<PietSharedState> pietSharedStates) {
    long startTime = System.nanoTime();
    initialBind(parameters.parentViewSupplier.get());
    FrameContext localFrameContext =
        createFrameContext(frame, frameWidthPx, pietSharedStates, checkNotNull(view));
    frameContext = localFrameContext;
    activeActions.clear();
    activeActions.addAll(frame.getActions().getOnHideActionsList());
    LinearLayout frameView = checkNotNull(view);

    try {
      for (Content content : frame.getContentsList()) {
        // For Slice we will create the lower level slice instead to remove the extra
        // level.
        List<ElementAdapter<?, ?>> adapters =
            getBoundAdaptersForContent(content, localFrameContext);
        for (ElementAdapter<?, ?> adapter : adapters) {
          childAdapters.add(adapter);
          setLayoutParamsOnChild(adapter);
          frameView.addView(adapter.getView());
        }
      }

      StyleProvider style = localFrameContext.makeStyleFor(frame.getStyleReferences());
      frameView.setBackground(style.createBackground());
    } catch (RuntimeException e) {
      // TODO: Remove this once error reporting is fully implemented.
      Logger.e(TAG, e, "Catch top level exception");
      String message = e.getMessage() != null ? e.getMessage() : GENERIC_EXCEPTION;
      if (e instanceof PietFatalException) {
        localFrameContext.reportMessage(
            MessageType.ERROR, ((PietFatalException) e).getErrorCode(), message);
      } else {
        localFrameContext.reportMessage(MessageType.ERROR, message);
      }
    }
    startTime = System.nanoTime() - startTime;
    // TODO: We should be targeting < 15ms and warn at 10ms?
    //   Until we get a chance to do the performance tuning, leave this at 30ms to prevent
    //   warnings on large GridRows based frames.
    if (startTime / 1000000 > 30) {
      Logger.w(
          TAG,
          localFrameContext.reportMessage(
              MessageType.WARNING,
              ERR_POOR_FRAME_RATE,
              String.format("Slow Bind (%s) time: %s ps", frame.getTag(), startTime / 1000)));
    }
    // If there were errors add an error slice to the frame
    if (localFrameContext.getDebugBehavior().getShowDebugViews()) {
      View errorView = localFrameContext.getDebugLogger().getReportView(MessageType.ERROR, context);
      if (errorView != null) {
        frameView.addView(errorView);
      }
      View warningView =
          localFrameContext.getDebugLogger().getReportView(MessageType.WARNING, context);
      if (warningView != null) {
        frameView.addView(warningView);
      }
    }
    eventLogger.logEvents(localFrameContext.getDebugLogger().getErrorCodes());
    LogDataCallback callback = parameters.hostProviders.getLogDataCallback();
    Frame localFrame = localFrameContext.getFrame();
    if (callback != null && localFrame != null && localFrame.hasLogData()) {
      callback.onBind(localFrame.getLogData(), frameView);
    }
  }

  @Override
  public void unbindModel() {
    triggerHideActions();

    LinearLayout view = checkNotNull(this.view);
    LogDataCallback callback = parameters.hostProviders.getLogDataCallback();
    if (frameContext != null) {
      Frame frame = frameContext.getFrame();
      if (callback != null && frame != null && frame.hasLogData()) {
        callback.onUnbind(frame.getLogData(), view);
      }
    }
    for (ElementAdapter<?, ?> child : childAdapters) {
      parameters.elementAdapterFactory.releaseAdapter(child);
    }
    childAdapters.clear();
    view.removeAllViews();
    frameContext = null;
  }

  private void setLayoutParamsOnChild(ElementAdapter<?, ?> childAdapter) {
    int width = childAdapter.getComputedWidthPx();
    width = width == StyleProvider.DIMENSION_NOT_SET ? LayoutParams.MATCH_PARENT : width;
    int height = childAdapter.getComputedHeightPx();
    height = height == StyleProvider.DIMENSION_NOT_SET ? LayoutParams.WRAP_CONTENT : height;

    LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(width, height);

    childAdapter.getElementStyle().applyMargins(context, params);

    params.gravity = childAdapter.getGravity(Gravity.TOP | Gravity.START);

    childAdapter.setLayoutParams(params);
  }

  @Override
  public LinearLayout getFrameContainer() {
    initialBind(parameters.parentViewSupplier.get());
    return checkNotNull(view);
  }

  @VisibleForTesting
  FrameContext createFrameContext(
      Frame frame, int frameWidthPx, List<PietSharedState> pietSharedStates, View frameView) {
    MediaQueryHelper mediaQueryHelper =
        new MediaQueryHelper(frameWidthPx, parameters.hostProviders.getAssetProvider(), context);
    PietStylesHelper pietStylesHelper =
        checkNotNull(parameters.pietStylesHelperFactory.get(pietSharedStates, mediaQueryHelper));
    return FrameContext.createFrameContext(
        frame,
        pietSharedStates,
        pietStylesHelper,
        debugBehavior,
        new DebugLogger(),
        actionHandler,
        parameters.hostProviders,
        frameView);
  }

  @Override
  public void triggerHideActions() {
    if (frameContext == null || view == null) {
      return;
    }
    FrameContext localFrameContext = frameContext;
    ViewUtils.triggerHideActions(
        view,
        localFrameContext.getFrame().getActions(),
        localFrameContext.getActionHandler(),
        localFrameContext.getFrame(),
        activeActions);

    for (ElementAdapter<?, ?> adapter : childAdapters) {
      adapter.triggerHideActions(localFrameContext);
    }
  }

  @Override
  public void triggerViewActions(View viewport) {
    if (frameContext == null || view == null) {
      return;
    }
    FrameContext localFrameContext = frameContext;
    ViewUtils.maybeTriggerViewActions(
        view,
        viewport,
        localFrameContext.getFrame().getActions(),
        localFrameContext.getActionHandler(),
        localFrameContext.getFrame(),
        activeActions);

    for (ElementAdapter<?, ?> adapter : childAdapters) {
      adapter.triggerViewActions(viewport, localFrameContext);
    }
  }

  @VisibleForTesting
  List<ElementAdapter<?, ?>> getBoundAdaptersForContent(
      Content content, FrameContext frameContext) {
    switch (content.getContentTypeCase()) {
      case ELEMENT:
        Element element = content.getElement();
        ElementAdapter<?, ?> inlineSliceAdapter =
            parameters.elementAdapterFactory.createAdapterForElement(element, frameContext);
        inlineSliceAdapter.bindModel(element, frameContext);
        return Collections.singletonList(inlineSliceAdapter);
      case TEMPLATE_INVOCATION:
        TemplateInvocation templateInvocation = content.getTemplateInvocation();
        ArrayList<ElementAdapter<?, ?>> returnList = new ArrayList<>();
        for (BindingContext bindingContext : templateInvocation.getBindingContextsList()) {
          TemplateAdapterModel model =
              new TemplateAdapterModel(
                  templateInvocation.getTemplateId(), frameContext, bindingContext);
          ElementAdapter<? extends View, ?> templateAdapter =
              parameters.templateBinder.createAndBindTemplateAdapter(model, frameContext);
          returnList.add(templateAdapter);
        }
        returnList.trimToSize();
        return Collections.unmodifiableList(returnList);
      default:
        Logger.wtf(
            TAG,
            frameContext.reportMessage(
                MessageType.ERROR,
                ERR_MISSING_OR_UNHANDLED_CONTENT,
                String.format(
                    "Unsupported Content type for frame: %s", content.getContentTypeCase())));
        return Collections.emptyList();
    }
  }

  @VisibleForTesting
  AdapterParameters getParameters() {
    return this.parameters;
  }

  @VisibleForTesting
  /*@Nullable*/
  LinearLayout getView() {
    return this.view;
  }

  private void initialBind(/*@Nullable*/ ViewGroup parent) {
    if (view != null) {
      return;
    }
    this.view = createView(parent);
  }

  private LinearLayout createView(/*@Nullable*/ ViewGroup parent) {
    LinearLayout linearLayout = new LinearLayout(context);
    linearLayout.setOrientation(LinearLayout.VERTICAL);
    ViewGroup.LayoutParams layoutParams;
    if (parent != null && parent.getLayoutParams() != null) {
      layoutParams = new LinearLayout.LayoutParams(parent.getLayoutParams());
      layoutParams.width = LayoutParams.MATCH_PARENT;
      layoutParams.height = LayoutParams.WRAP_CONTENT;
    } else {
      layoutParams = new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
    }
    linearLayout.setLayoutParams(layoutParams);
    return linearLayout;
  }
}
