// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.host;

import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.ActionsBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.ChunkedTextBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.CustomBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.ElementBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.GridCellWidthBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.ImageBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.LogDataBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.ParameterizedTextBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.StyleBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.TemplateBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.VisibilityBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.BindingValue;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.HostBindingData;

/**
 * Interface which allows a host to provide bindings to Piet directly. This specifically allows
 * changing bindings based on on-device information.
 *
 * <p>Methods are called by Piet during the binding process when their associated *BindingRef is
 * found and the server has specified a {@link BindingValue} which includes a {@link
 * HostBindingData} set.
 *
 * <p>Methods returns should include the associated binding filled in with no {@link
 * HostBindingData}. If a {@link HostBindingData} is set on the returned {@link BindingValue}
 * instances then it will be ignored.
 *
 * <p>This class provides a default implementation which just removes {@link HostBindingData} from
 * any server specified {@link HostBindingData}.
 *
 * <p>See [INTERNAL LINK].
 */
public class HostBindingProvider {
    /**
     * Called by Piet during the binding process for a {@link CustomBindingRef} if the server has
     * specified a BindingValue which includes {@link HostBindingData} set.
     */
    public BindingValue getCustomElementDataBindingForValue(BindingValue bindingValue) {
        return clearHostBindingData(bindingValue);
    }

    /**
     * Called by Piet during the binding process for a {@link ParameterizedTextBindingRef} if the
     * server has specified a BindingValue which includes {@link HostBindingData} set.
     */
    public BindingValue getParameterizedTextBindingForValue(BindingValue bindingValue) {
        return clearHostBindingData(bindingValue);
    }

    /**
     * Called by Piet during the binding process for a {@link ChunkedTextBindingRef} if the server
     * has specified a BindingValue which includes {@link HostBindingData} set.
     */
    public BindingValue getChunkedTextBindingForValue(BindingValue bindingValue) {
        return clearHostBindingData(bindingValue);
    }

    /**
     * Called by Piet during the binding process for a {@link ImageBindingRef} if the server has
     * specified a BindingValue which includes {@link HostBindingData} set.
     */
    public BindingValue getImageBindingForValue(BindingValue bindingValue) {
        return clearHostBindingData(bindingValue);
    }

    /**
     * Called by Piet during the binding process for a {@link ActionsBindingRef} if the server has
     * specified a BindingValue which includes {@link HostBindingData} set.
     */
    public BindingValue getActionsBindingForValue(BindingValue bindingValue) {
        return clearHostBindingData(bindingValue);
    }

    /**
     * Called by Piet during the binding process for a {@link GridCellWidthBindingRef} if the server
     * has specified a BindingValue which includes {@link HostBindingData} set.
     */
    public BindingValue getGridCellWidthBindingForValue(BindingValue bindingValue) {
        return clearHostBindingData(bindingValue);
    }

    /**
     * Called by Piet during the binding process for a {@link ElementBindingRef} if the server has
     * specified a BindingValue which includes {@link HostBindingData} set.
     */
    public BindingValue getElementBindingForValue(BindingValue bindingValue) {
        return clearHostBindingData(bindingValue);
    }

    /**
     * Called by Piet during the binding process for a {@link LogDataBindingRef} if the server has
     * specified a BindingValue which includes {@link HostBindingData} set.
     */
    public BindingValue getLogDataBindingForValue(BindingValue bindingValue) {
        return clearHostBindingData(bindingValue);
    }

    /**
     * Called by Piet during the binding process for a {@link TemplateBindingRef} if the server has
     * specified a BindingValue which includes {@link HostBindingData} set.
     */
    public BindingValue getTemplateBindingForValue(BindingValue bindingValue) {
        return clearHostBindingData(bindingValue);
    }

    /**
     * Called by Piet during the binding process for a {@link StyleBindingRef} if the server has
     * specified a BindingValue which includes {@link HostBindingData} set.
     */
    public BindingValue getStyleBindingForValue(BindingValue bindingValue) {
        return clearHostBindingData(bindingValue);
    }

    /**
     * Called by Piet during the binding process for a {@link VisibilityBindingRef} if the server
     * has specified a BindingValue which includes {@link HostBindingData} set.
     */
    public BindingValue getVisibilityBindingForValue(BindingValue bindingValue) {
        return clearHostBindingData(bindingValue);
    }

    private BindingValue clearHostBindingData(BindingValue bindingValue) {
        return bindingValue.toBuilder().clearHostBindingData().build();
    }
}
