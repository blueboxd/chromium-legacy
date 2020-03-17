// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/text_direction.mojom-lite.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
import 'chrome://new-tab-page/skcolor.mojom-lite.js';
import 'chrome://new-tab-page/new_tab_page.mojom-lite.js';

import {BrowserProxy} from 'chrome://new-tab-page/browser_proxy.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';

/** @type {string} */
export const NONE_ANIMATION = 'none 0s ease 0s 1 normal none running';

/**
 * @param {!HTMLElement} element
 * @param {string} key
 */
export function keydown(element, key) {
  keyDownOn(element, '', [], key);
}

/**
 * Asserts the computed style value for an element.
 * @param {!HTMLElement} element The element.
 * @param {string} name The name of the style to assert.
 * @param {string} expected The expected style value.
 */
export function assertStyle(element, name, expected) {
  const actual = window.getComputedStyle(element).getPropertyValue(name).trim();
  assertEquals(expected, actual);
}

/**
 * Asserts the computed style for an element is not value.
 * @param {!HTMLElement} element The element.
 * @param {string} name The name of the style to assert.
 * @param {string} not The value the style should not be.
 */
export function assertNotStyle(element, name, not) {
  const actual = window.getComputedStyle(element).getPropertyValue(name).trim();
  assertNotEquals(not, actual);
}

/**
 * Asserts that an element is focused.
 * @param {!HTMLElement} element The element to test.
 */
export function assertFocus(element) {
  assertEquals(element, getDeepActiveElement());
}

/**
 * Creates a mocked test proxy.
 * @return {TestBrowserProxy}
 */
export function createTestProxy() {
  const testProxy = TestBrowserProxy.fromClass(BrowserProxy);
  testProxy.callbackRouter = new newTabPage.mojom.PageCallbackRouter();
  testProxy.callbackRouterRemote =
      testProxy.callbackRouter.$.bindNewPipeAndPassRemote();
  testProxy.handler =
      TestBrowserProxy.fromClass(newTabPage.mojom.PageHandlerRemote);
  testProxy.setResultFor('createUntrustedIframeSrc', '');
  return testProxy;
}
