// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SWIPE_FINISH_THRESHOLD_PX, SWIPE_START_THRESHOLD_PX, TabSwiper, TRANSLATE_ANIMATION_THRESHOLD_PX} from 'chrome://tab-strip/tab_swiper.js';

import {eventToPromise} from '../test_util.m.js';

import {TestTabsApiProxy} from './test_tabs_api_proxy.js';

suite('TabSwiper', () => {
  let tabElement;
  let tabSwiper;

  const tab = {id: 1001};

  setup(() => {
    document.body.innerHTML = '';

    tabElement = document.createElement('div');
    tabElement.tab = tab;
    document.body.appendChild(tabElement);

    tabSwiper = new TabSwiper(tabElement);
    tabSwiper.startObserving();
  });

  test('swiping progresses the animation', () => {
    // Set margin top 0 to avoid offsetting the bounding client rect.
    document.body.style.margin = 0;

    const tabWidth = 100;
    document.body.style.setProperty('--tabstrip-tab-width', `${tabWidth}px`);

    const tabElStyle = window.getComputedStyle(tabElement);

    const startY = 50;
    const pointerState = {clientY: startY, pointerId: 1};
    tabElement.dispatchEvent(new PointerEvent('pointerdown', pointerState));

    // Swipe was not enough to start any part of the animation.
    pointerState.clientY = startY - 1;
    pointerState.movementY = 1; /* Any non-0 value here is fine. */
    tabElement.dispatchEvent(new PointerEvent('pointermove', pointerState));
    assertEquals(tabElStyle.maxWidth, `${tabWidth}px`);
    assertEquals(tabElStyle.opacity, '1');
    let startTop = tabElement.getBoundingClientRect().top;
    assertEquals(startTop, 0);

    // Swipe was enough to start animating the position.
    pointerState.clientY = startY - (TRANSLATE_ANIMATION_THRESHOLD_PX + 1);
    tabElement.dispatchEvent(new PointerEvent('pointermove', pointerState));
    assertEquals(tabElStyle.maxWidth, `${tabWidth}px`);
    let top = tabElement.getBoundingClientRect().top;
    assertTrue(top < startTop && top > -1 * SWIPE_FINISH_THRESHOLD_PX);

    // Swipe was enough to start animating max width and opacity.
    pointerState.clientY = startY - (SWIPE_START_THRESHOLD_PX + 1);
    tabElement.dispatchEvent(new PointerEvent('pointermove', pointerState));
    assertTrue(
        parseInt(tabElStyle.maxWidth) > 0 &&
        parseInt(tabElStyle.maxWidth) < tabWidth);
    assertTrue(
        parseFloat(tabElStyle.opacity) > 0 &&
        parseFloat(tabElStyle.opacity) < 1);

    // Swipe was enough to finish animating.
    pointerState.clientY = startY - (SWIPE_FINISH_THRESHOLD_PX + 1);
    tabElement.dispatchEvent(new PointerEvent('pointermove', pointerState));
    assertEquals(tabElStyle.maxWidth, '0px');
    assertEquals(tabElStyle.opacity, '0');
    assertEquals(
        tabElement.getBoundingClientRect().top, -SWIPE_FINISH_THRESHOLD_PX);
  });

  test('finishing the swipe animation fires an event', async () => {
    const firedEventPromise = eventToPromise('swipe', tabElement);
    const startY = 50;
    const pointerState = {clientY: startY, pointerId: 1};
    tabElement.dispatchEvent(new PointerEvent('pointerdown', pointerState));

    pointerState.clientY = startY - (SWIPE_FINISH_THRESHOLD_PX + 1);
    pointerState.movementY = 1; /* Any non-0 value here is fine. */
    tabElement.dispatchEvent(new PointerEvent('pointermove', pointerState));
    tabElement.dispatchEvent(new PointerEvent('pointerup', pointerState));
    await firedEventPromise;
  });

  test('swiping enough and releasing finishes the animation', async () => {
    const firedEventPromise = eventToPromise('swipe', tabElement);

    const tabElStyle = window.getComputedStyle(tabElement);
    const startY = 50;

    const pointerState = {clientY: 50, pointerId: 1};
    tabElement.dispatchEvent(new PointerEvent('pointerdown', pointerState));

    pointerState.clientY = startY - (SWIPE_START_THRESHOLD_PX + 1);
    pointerState.movementY = 1; /* Any non-0 value here is fine. */
    tabElement.dispatchEvent(new PointerEvent('pointermove', pointerState));
    tabElement.dispatchEvent(new PointerEvent('pointerup', pointerState));
    await firedEventPromise;
    assertEquals(tabElStyle.maxWidth, '0px');
    assertEquals(tabElStyle.opacity, '0');
  });

  test('swiping and letting go before resets animation', () => {
    tabElement.style.setProperty('--tabstrip-tab-width', '100px');
    const tabElStyle = window.getComputedStyle(tabElement);
    const startY = 50;

    const pointerState = {clientY: 50, pointerId: 1};
    tabElement.dispatchEvent(new PointerEvent('pointerdown', pointerState));

    pointerState.clientY = startY - 1;
    pointerState.movementY = 1; /* Any non-0 value here is fine. */
    tabElement.dispatchEvent(new PointerEvent('pointermove', pointerState));
    tabElement.dispatchEvent(new PointerEvent('pointerup', pointerState));

    assertEquals(tabElStyle.maxWidth, '100px');
    assertEquals(tabElStyle.opacity, '1');
  });

  test('swiping fast enough finishes playing the animation', async () => {
    const tabElStyle = window.getComputedStyle(tabElement);
    const firedEventPromise = eventToPromise('swipe', tabElement);
    const startY = 50;
    const pointerState = {clientY: 50, pointerId: 1};

    tabElement.dispatchEvent(new PointerEvent('pointerdown', pointerState));

    pointerState.clientY = -100;
    pointerState.movementY = -50;
    pointerState.timestamp = 1020;
    tabElement.dispatchEvent(new PointerEvent('pointermove', pointerState));
    tabElement.dispatchEvent(new PointerEvent('pointerup', pointerState));

    await firedEventPromise;
    assertEquals(tabElStyle.maxWidth, '0px');
    assertEquals(tabElStyle.opacity, '0');
  });

  test('pointerdown should reset the animation time', async () => {
    tabElement.style.setProperty('--tabstrip-tab-width', '100px');
    const tabElStyle = window.getComputedStyle(tabElement);
    const pointerState = {clientY: 50, pointerId: 1};
    tabElement.dispatchEvent(new PointerEvent('pointerdown', pointerState));

    // Mimic a swipe that turns into a scroll.
    pointerState.clientY += SWIPE_FINISH_THRESHOLD_PX;
    pointerState.movementY = 1; /* Any non-0 value here is fine. */
    tabElement.dispatchEvent(new PointerEvent('pointermove', pointerState));
    tabElement.dispatchEvent(new PointerEvent('pointerleave', pointerState));

    // Mimic a tap.
    pointerState.clientY = 50;
    tabElement.dispatchEvent(new PointerEvent('pointerdown', pointerState));

    // Style should reset to defaults.
    assertEquals(tabElStyle.maxWidth, '100px');
    assertEquals(tabElStyle.opacity, '1');
  });
});
