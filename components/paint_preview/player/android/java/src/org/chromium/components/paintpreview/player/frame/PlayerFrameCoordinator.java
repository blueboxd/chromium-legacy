// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;

import org.chromium.base.UnguessableToken;
import org.chromium.components.paintpreview.player.PlayerCompositorDelegate;
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
            UnguessableToken frameGuid, int contentWidth, int contentHeight,
            boolean canDetectZoom) {
        PropertyModel model = new PropertyModel.Builder(PlayerFrameProperties.ALL_KEYS).build();
        mMediator = new PlayerFrameMediator(
                model, compositorDelegate, frameGuid, contentWidth, contentHeight);
        mView = new PlayerFrameView(context, canDetectZoom, mMediator);
        PropertyModelChangeProcessor.create(model, mView, PlayerFrameViewBinder::bind);
    }

    /**
     * Adds a child {@link PlayerFrameCoordinator} to this class.
     * @param subFrame The sub-frame's {@link PlayerFrameCoordinator}.
     * @param clipRect The {@link Rect} in which this sub-frame should be shown in.
     */
    public void addSubFrame(PlayerFrameCoordinator subFrame, Rect clipRect) {
        mMediator.addSubFrame(subFrame.getView(), clipRect);
    }

    /**
     * @return The view associated with this component.
     */
    public View getView() {
        return mView;
    }
}
