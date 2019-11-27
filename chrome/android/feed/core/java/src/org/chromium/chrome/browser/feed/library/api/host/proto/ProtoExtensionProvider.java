// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.proto;

import com.google.protobuf.GeneratedMessageLite.GeneratedExtension;

import java.util.List;

/** Allows the host application to register proto extensions in the Feed's global registry. */
public interface ProtoExtensionProvider {
    /**
     * The Feed will call this method on startup. Any proto extensions that will need to be
     * serialized by the Feed should be returned at that time.
     *
     * @return a list of the proto extensions the host application will use in the Feed.
     */
    List<GeneratedExtension<?, ?>> getProtoExtensions();
}
