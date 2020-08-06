// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_TOKENS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_TOKENS_H_

#include "base/util/type_safety/token_type.h"
#include "third_party/blink/public/common/tokens/multi_token.h"

namespace blink {

// Various token types. These are used as cross-layer and cross-process
// identifiers for objects that exist in blink, but which have representations
// in the browser process. They should not be used to identify objects in
// browser-to-renderer control messages; rather, such messages should exist as
// methods on the interface bound to the object itself. They are fine to use
// for informational messages that cross over other interfaces, in both
// directions.
//
// See README.md for more details.

////////////////////////////////////////////////////////////////////////////////
// FRAME TOKENS

// Uniquely identifies a blink::LocalFrame / blink::WebLocalFrame /
// content::RenderFrame in a renderer process, and its content::RenderFrameHost
// counterpart in the browser.
using LocalFrameToken = util::TokenType<class LocalFrameTokenTypeMarker>;

// Uniquely identifies an blink::RemoteFrame / blink::WebRemoteFrame /
// content::RenderFrameProxy in a renderer process, and its
// ontent::RenderFrameProxyHost counterpart in the browser. There can be
// multiple RemoteFrames corresponding to a single LocalFrame, and each token
// will be distinct.
using RemoteFrameToken = util::TokenType<class RemoteFrameTokenTypeMarker>;

// Can represent either type of FrameToken.
using FrameToken = MultiToken<LocalFrameToken, RemoteFrameToken>;

////////////////////////////////////////////////////////////////////////////////
// WORKER TOKENS

// Identifies a blink::DedicatedWorkerGlobalScope in the renderer and a
// content::DedicatedWorkerHost in the browser.
using DedicatedWorkerToken =
    util::TokenType<class DedicatedWorkerTokenTypeMarker>;

// Identifies a blink::ServiceWorkerGlobalScope in the renderer and a
// content::ServiceWorkerVersion in the browser.
using ServiceWorkerToken = util::TokenType<class ServiceWorkerTokenTypeMarker>;

// Identifies a blink::SharedWorkerGlobalScope in the renderer and a
// content::SharedWorkerHost in the browser.
using SharedWorkerToken = util::TokenType<class SharedWorkerTokenTypeMarker>;

// Can represent any type of WorkerToken.
using WorkerToken =
    MultiToken<DedicatedWorkerToken, ServiceWorkerToken, SharedWorkerToken>;

////////////////////////////////////////////////////////////////////////////////
// OTHER TOKENS
//
// Keep this section last.
//
// If you have multiple tokens that make a thematic group, please lift them to
// their own section, in alphabetical order. If adding a new token here, please
// keep the following list in alphabetic order.

// Identifies an ExecutionContext hosted in a renderer for the purposes of
// memory and CPU attribution. Worklets are not tracked independently, but
// simply attributed to their parent context, hence only LocalFrames and workers
// can be named. As such, it is possible for multiple ExecutionContexts to have
// the same ExecutionContextAttributionToken.
using ExecutionContextAttributionToken = MultiToken<LocalFrameToken,
                                                    DedicatedWorkerToken,
                                                    ServiceWorkerToken,
                                                    SharedWorkerToken>;

// Identifies a blink::PortalContents / blink::HTMLPortalElement in the
// renderer process, and a content::Portal in the browser process.
using PortalToken = util::TokenType<class PortalTokenTypeMarker>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_TOKENS_H_
