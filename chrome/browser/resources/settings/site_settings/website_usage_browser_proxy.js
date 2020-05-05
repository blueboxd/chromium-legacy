// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
// clang-format on

/** @interface */
export class WebsiteUsageBrowserProxy {
  /** @param {string} host */
  fetchUsageTotal(host) {}

  /** @param {string} origin */
  clearUsage(origin) {}
}

/** @implements {WebsiteUsageBrowserProxy} */
export class WebsiteUsageBrowserProxyImpl {
  /** @override */
  fetchUsageTotal(host) {
    chrome.send('fetchUsageTotal', [host]);
  }

  /** @override */
  clearUsage(origin) {
    chrome.send('clearUsage', [origin]);
  }
}

addSingletonGetter(WebsiteUsageBrowserProxyImpl);
