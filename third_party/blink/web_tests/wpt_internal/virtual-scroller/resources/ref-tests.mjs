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

/**
 * Tests scrolling in 2 steps to the end of the page.
 * This reproduces crbug.com/1004102.
 */
export function testFullScrollToEndIn2Steps(target) {
  helpers.appendDivs(target, MORE_THAN_SCREENFUL, '10px');
  // TODO(fergal): rewrite tests to use await.
  // Give the scroller time to settle.
  helpers.inNFrames(10, () => {
    target.children[1].scrollIntoView(/* alignToTop= */ true);
    // Give the scroller time to settle.
    helpers.inNFrames(10, () => {
      window.scrollBy(0, target.getBoundingClientRect().height);
      helpers.stopWaiting();
    });
  });
};

export function testLargeChild(target) {
  // This scrollTo and nextFrame are not necessary for the ref-test
  // however it helps when trying to debug this test in a
  // browser. Without it, the scroll offset may be preserved across
  // page reloads.
  document.body.scrollTo(0, 0);
  helpers.nextFrame(() => {
    const largeChild = helpers.largeDiv('largeChild');
    target.appendChild(largeChild);
    const child = helpers.div('child');
    target.appendChild(child);

    // Give the scroller time to settle.
    helpers.inNFrames(10, () => {
      window.scrollBy(0, largeChild.getBoundingClientRect().height);
      helpers.stopWaiting();
    });
  });
}

export function testLargeChildComment(target) {
  // This scrollTo and nextFrame are not necessary for the ref-test
  // however it helps when trying to debug this test in a
  // browser. Without it, the scroll offset may be preserved across
  // page reloads.
  document.body.scrollTo(0, 0);
  helpers.nextFrame(() => {
    const largeChild = helpers.largeDiv('largeChild');
    target.appendChild(largeChild);
    // Ensure that non-element nodes don't cause problems.
    target.appendChild(document.createComment('comment'));
    target.appendChild(helpers.div('child'));

    // Give the scroller time to settle.
    helpers.inNFrames(10, () => {
      window.scrollBy(0, largeChild.getBoundingClientRect().height);
      helpers.stopWaiting();
    });
  });
}

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

export function testScrollFromOffScreen(target) {
  // This scrollTo and nextFrame are not necessary for the ref-test
  // however it helps when trying to debug this test in a
  // browser. Without it, the scroll offset may be preserved across
  // page reloads.
  document.body.scrollTo(0, 0);
  helpers.nextFrame(() => {
    // The page is a large element (much bigger than the page)
    // followed by the scroller. We then scroll down to the scroller.
    const largeSibling = helpers.largeDiv('large');
    target.before(largeSibling);
    const child = helpers.div('child');
    target.appendChild(child);

    // Give the scroller time to settle.
    helpers.inNFrames(10, () => {
      window.scrollBy(0, largeSibling.getBoundingClientRect().height);
      helpers.stopWaiting();
    });
  });
}

/**
 * Make sure that an element that was hidden by the scroller does not
 * remain hidden if it is moved out of the scroller.
 */
export function testUnlockAfterRemove(target) {
  // This scrollTo and nextFrame are not necessary for the ref-test
  // however it helps when trying to debug this test in a
  // browser. Without it, the scroll offset may be preserved across
  // page reloads.
  document.body.scrollTo(0, 0);
  helpers.nextFrame(() => {
    helpers.appendDivs(target, MORE_THAN_SCREENFUL, '10px');
    helpers.nextFrame(() => {
      const e = target.lastElementChild;
      // Make sure the element can stay locked outside of the scroller.
      e.style.contain = 'style layout';
      target.parentElement.appendChild(e);

      helpers.nextFrame(() => {
        window.scrollBy(0, target.getBoundingClientRect().height);
        helpers.stopWaiting();
      });
    });
  });
};

/**
 * Runs |test| with a <virtual-scroller>, waiting until the custom element is
 * defined.
 */
export function withVirtualScroller(test) {
  customElements.whenDefined('virtual-scroller').then(() => {
    runTest('virtual-scroller', test);
  });
}

/**
 * Runs |test| with a <div>.
 */
export function withDiv(test) {
  runTest('div', test);
}

function runTest(elementName, test) {
  const element = document.createElement(elementName);
  document.body.appendChild(element);
  test(element);
}
