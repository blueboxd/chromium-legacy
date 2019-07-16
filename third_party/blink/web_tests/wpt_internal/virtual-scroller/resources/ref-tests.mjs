/**
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * @fileoverview Runs a ref-test based on the filename.
 * @package
 */

import * as helpers from './helpers.mjs';

const LESS_THAN_SCREENFUL = 5;
const MORE_THAN_SCREENFUL = 100;
const WORDS_IN_PARAGRAPH = 50;

export function testFull(target) {
  helpers.appendDivs(target, MORE_THAN_SCREENFUL, '10px');
  helpers.stopWaiting();
}

export function testFullScroll500px(target) {
  helpers.appendDivs(target, MORE_THAN_SCREENFUL, '10px');
  helpers.nextFrame(() => {
    window.scrollBy(0, 500);  // eslint-disable-line no-magic-numbers
    helpers.stopWaiting();
  });
};

export function testMoveElement(target) {
  helpers.appendDivs(target, MORE_THAN_SCREENFUL, '10px');
  helpers.nextFrame(() => {
    const element = target.firstElementChild;
    target.appendChild(element);
    helpers.stopWaiting();
  });
}

export function testMoveElementScrollIntoView(target) {
  helpers.appendDivs(target, MORE_THAN_SCREENFUL, '10px');
  const element = target.firstElementChild;
  target.appendChild(element);
  helpers.nextFrame(() => {
    element.scrollIntoView();
    helpers.stopWaiting();
  });
}

export function testPart(target) {
  helpers.appendDivs(target, LESS_THAN_SCREENFUL, '10px');
  helpers.stopWaiting();
}

export function testResize(target) {
  target.style.overflow = 'hidden';
  target.style.width = '500px';
  helpers.appendDivs(target, MORE_THAN_SCREENFUL, '10px');
  const text = helpers.words(WORDS_IN_PARAGRAPH);
  for (const e of target.children) {
    e.innerText = text;
  }
  helpers.nextFrame(() => {
    target.style.width = '300px';
    helpers.stopWaiting();
  });
}

/**
 * Runs |test| with a <virtual-scroller>, waiting until the custom element is
 * defined.
*/
export function withVirtualScroller(test) {
  customElements.whenDefined('virtual-scroller').then(() => {
    runTest("virtual-scroller", test);
  });
}

/**
 * Runs |test| with a <div>.
*/
export function withDiv(test) {
  runTest("div", test);
}

function runTest(elementName, test) {
  const element = document.createElement(elementName);
  document.body.appendChild(element);
  test(element);
}
