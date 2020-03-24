// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Helpers for testing against DOM. For integration tests, this is
 * injected into an isolated world, so can't access objects in other scripts.
 */

/**
 * Repeatedly runs a query selector until it finds an element.
 *
 * @param {string} query
 * @param {!Array<string>=} opt_path
 * @return {Promise<!Element>}
 */
async function waitForNode(query, opt_path) {
  /** @type {!HTMLElement|!ShadowRoot} */
  let node = document.body;
  const parent = opt_path ? opt_path.shift() : undefined;
  if (parent) {
    const element = await waitForNode(parent, opt_path);
    if (!(element instanceof HTMLElement) || !element.shadowRoot) {
      throw new Error('Path not a shadow root HTMLElement');
    }
    node = element.shadowRoot;
  }
  const existingElement = node.querySelector(query);
  if (existingElement) {
    return Promise.resolve(existingElement);
  }
  console.log('Waiting for ' + query);
  return new Promise(resolve => {
    const observer = new MutationObserver((mutationList, observer) => {
      const element = node.querySelector(query);
      if (element) {
        resolve(element);
        observer.disconnect();
      }
    });
    observer.observe(node, {childList: true, subtree: true});
  });
}
