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

import android.view.View;
import android.widget.LinearLayout;
import com.google.search.now.ui.piet.PietAndroidSupport.ShardingControl;
import com.google.search.now.ui.piet.PietProto.Frame;
import com.google.search.now.ui.piet.PietProto.PietSharedState;
import java.util.List;

/**
 * An adapter which manages {@link Frame} instances. Frames will contain one or more slices. This
 * class has additional public methods to support host access to the primary view of the frame
 * before the model is bound to the frame. A frame is basically a vertical LinearLayout of slice
 * Views which are created by {@link ElementAdapter}. This Adapter is not created through a Factory
 * and is managed by the host.
 */
public interface FrameAdapter {

  /**
   * This version of bind will support the {@link ShardingControl}. Sharding allows only part of the
   * frame to be rendered. When sharding is used, a frame is one or more LinearLayout containing a
   * subset of the full set of slices defined for the frame.
   */
  void bindModel(
      Frame frame,
      int frameWidthPx,
      /*@Nullable*/ ShardingControl shardingControl,
      List<PietSharedState> pietSharedStates);

  void unbindModel();

  /**
   * Return the LinearLayout managed by this FrameAdapter. This method can be used to gain access to
   * this view before {@code bindModel} is called.
   */
  LinearLayout getFrameContainer();

  /**
   * Trigger any actions that occur on the view being hidden. This is also called by unbind(); it
   * only needs to be called independently in cases where the view goes offscreen but stays bound.
   * Piet hide/show action handling is idempotent, so calling this multiple times in a row will only
   * trigger each action once.
   */
  void triggerHideActions();

  /** Trigger all visibility-related actions, filtered on the actual visibility of the view. */
  void triggerViewActions(View viewport);
}
