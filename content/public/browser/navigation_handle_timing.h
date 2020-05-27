// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATION_HANDLE_TIMING_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATION_HANDLE_TIMING_H_

#include "base/time/time.h"
#include "content/common/content_export.h"

namespace content {

// NavigationHandleTiming contains timing information of loading for navigation
// recorded in NavigationHandle. This is used for UMAs, not exposed to
// JavaScript via Navigation Timing API etc unlike mojom::NavigationTiming. See
// the design doc for details.
// https://docs.google.com/document/d/16oqu9lyPbfgZIjQsRaCfaKE8r1Cdlb3d4GVSdth4AN8/edit?usp=sharing
struct CONTENT_EXPORT NavigationHandleTiming {
  // The time the first HTTP request was sent. This is filled with
  // net::LoadTimingInfo::send_start during navigation.
  //
  // In some cases, this can be the time an internal request started that did
  // not go to the networking layer. For example,
  // - Service Worker: the time the fetch event was ready to be dispatched, see
  //   content::ServiceWorkerNavigationLoader::DidPrepareFetchEvent()).
  // - HSTS: the time the internal redirect was handled.
  // - Signed Exchange: the time the SXG was handled.
  base::TimeTicks first_request_start_time;

  // The time the headers of the first HTTP response were received. This is
  // filled with net::LoadTimingInfo::receive_headers_start on the first HTTP
  // response during navigation.
  //
  // In some cases, this can be the time an internal response was received that
  // did not come from the networking layer. For example,
  // - Service Worker: the time the response from the service worker was
  //   received, see content::ServiceWorkerNavigationLoader::StartResponse().
  // - HSTS: the time the internal redirect was handled.
  // - Signed Exchange: the time the SXG was handled.
  base::TimeTicks first_response_start_time;

  // The time a callback for the navigation loader was first invoked. The time
  // between this and |first_response_start_time| includes any throttling or
  // process/thread hopping between the network stack receiving the response and
  // the navigation loader receiving it.
  base::TimeTicks first_loader_callback_time;

  // The time the navigation commit message was sent to a renderer process.
  base::TimeTicks navigation_commit_sent_time;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NAVIGATION_HANDLE_TIMING_H_
