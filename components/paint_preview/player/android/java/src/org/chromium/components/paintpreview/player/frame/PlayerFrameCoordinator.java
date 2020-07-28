// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.content.Context;
import android.graphics.Rect;
import android.util.Size;
import android.view.View;
import android.view.ViewConfiguration;
import android.widget.OverScroller;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.UnguessableToken;
import org.chromium.components.paintpreview.player.OverscrollHandler;
import org.chromium.components.paintpreview.player.PlayerCompositorDelegate;
import org.chromium.components.paintpreview.player.PlayerGestureListener;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Sets up the view and the logic behind it for a Paint Preview frame.
 */
public class PlayerFrameCoordinator {
    private PlayerFrameMediator mMediator;
    private PlayerFrameView mView;

    /**
     * Creates a {@link PlayerFrameMediator} and {@link PlayerFrameView} for this component and
     * binds them together.
     */
    public PlayerFrameCoordinator(Context context, PlayerCompositorDelegate compositorDelegate,
            UnguessableToken frameGuid, int contentWidth, int contentHeight, int initialScrollX,
            int initialScrollY, boolean canDetectZoom,
            @Nullable OverscrollHandler overscrollHandler, PlayerGestureListener gestureHandler) {
        PropertyModel model = new PropertyModel.Builder(PlayerFrameProperties.ALL_KEYS).build();
        OverScroller scroller = new OverScroller(context);
        scroller.setFriction(ViewConfiguration.getScrollFriction() / 2);

        mMediator = new PlayerFrameMediator(model, compositorDelegate, gestureHandler, frameGuid,
                new Size(contentWidth, contentHeight), initialScrollX, initialScrollY);

        PlayerFrameScaleController scaleController = null;
        if (canDetectZoom) {
            scaleController =
                    new PlayerFrameScaleController(model.get(PlayerFrameProperties.SCALE_MATRIX),
                            mMediator, gestureHandler::onScale);
        }
        PlayerFrameScrollController scrollController = new PlayerFrameScrollController(
                scroller, mMediator, gestureHandler::onScroll, gestureHandler::onFling);
        PlayerFrameGestureDetectorDelegate gestureDelegate = new PlayerFrameGestureDetectorDelegate(
                scaleController, scrollController, mMediator);

        mView = new PlayerFrameView(context, canDetectZoom, mMediator, gestureDelegate);
        if (overscrollHandler != null) {
            scrollController.setOverscrollHandler(overscrollHandler);
        }
        PropertyModelChangeProcessor.create(model, mView, PlayerFrameViewBinder::bind);
    }

    /**
     * Adds a child {@link PlayerFrameCoordinator} to this class.
     * @param subFrame The sub-frame's {@link PlayerFrameCoordinator}.
     * @param clipRect The {@link Rect} in which this sub-frame should be shown in.
     */
    public void addSubFrame(PlayerFrameCoordinator subFrame, Rect clipRect) {
        mMediator.addSubFrame(subFrame.mView, clipRect, subFrame.getMediator());
        subFrame.mView.getGestureDetector().setParentGestureDetector(mView.getGestureDetector());
    }

    /**
     * @return The mediator associated with this component.
     */
    public PlayerFrameMediator getMediator() {
        return mMediator;
    }

    /**
     * @return The view associated with this component.
     */
    public View getView() {
        return mView;
    }

    @VisibleForTesting
    public boolean checkRequiredBitmapsLoadedForTest() {
        return mMediator.checkRequiredBitmapsLoadedForTest();
    }
}
