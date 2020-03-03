// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// So that mojo is defined.
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {createScrollBorders, hexColorToSkColor, skColorToRgb} from 'chrome://new-tab-page/utils.js';
import {waitAfterNextRender} from 'chrome://test/test_util.m.js';

suite('NewTabPageUtilsTest', () => {
  test('Can convert simple SkColors to rgb strings', () => {
    assertEquals(skColorToRgb({value: 0xffff0000}), 'rgb(255, 0, 0)');
    assertEquals(skColorToRgb({value: 0xff00ff00}), 'rgb(0, 255, 0)');
    assertEquals(skColorToRgb({value: 0xff0000ff}), 'rgb(0, 0, 255)');
    assertEquals(skColorToRgb({value: 0xffffffff}), 'rgb(255, 255, 255)');
    assertEquals(skColorToRgb({value: 0xff000000}), 'rgb(0, 0, 0)');
  });

  test('Can convert complex SkColors to rgb strings', () => {
    assertEquals(skColorToRgb({value: 0xffa11f8f}), 'rgb(161, 31, 143)');
    assertEquals(skColorToRgb({value: 0xff2b6335}), 'rgb(43, 99, 53)');
    assertEquals(skColorToRgb({value: 0xffe3d2c1}), 'rgb(227, 210, 193)');
  });

  test('Cannot convert transparent SkColors to rgb strings', () => {
    assertEquals(skColorToRgb({value: 0xfeabcdef}), 'rgb(0, 0, 0)');
  });

  test('Can convert simple hex strings to SkColors', () => {
    assertDeepEquals(hexColorToSkColor('#ff0000'), {value: 0xffff0000});
    assertDeepEquals(hexColorToSkColor('#00ff00'), {value: 0xff00ff00});
    assertDeepEquals(hexColorToSkColor('#0000ff'), {value: 0xff0000ff});
    assertDeepEquals(hexColorToSkColor('#ffffff'), {value: 0xffffffff});
    assertDeepEquals(hexColorToSkColor('#000000'), {value: 0xff000000});
  });

  test('Can convert complex hex strings to SkColors', () => {
    assertDeepEquals(hexColorToSkColor('#a11f8f'), {value: 0xffa11f8f});
    assertDeepEquals(hexColorToSkColor('#2b6335'), {value: 0xff2b6335});
    assertDeepEquals(hexColorToSkColor('#e3d2c1'), {value: 0xffe3d2c1});
  });

  test('Cannot convert malformed hex strings to SkColors', () => {
    assertDeepEquals(hexColorToSkColor('#fffffr'), {value: 0});
    assertDeepEquals(hexColorToSkColor('#ffffff0'), {value: 0});
    assertDeepEquals(hexColorToSkColor('ffffff'), {value: 0});
  });
});

suite('scroll borders', () => {
  /** @type {!HTMLElement} */
  let container;
  /** @type {!HTMLElement} */
  let content;
  /** @type {!HTMLElement} */
  let top;
  /** @type {!HTMLElement} */
  let bottom;
  /** @type {!IntersectionObserver} */
  let observer;

  /** @param {!HTMLElement} el */
  function assertHidden(el) {
    assertTrue(el.matches('[scroll-border]:not([show])'));
  }

  /** @param {!HTMLElement} el */
  function assertShown(el) {
    assertTrue(el.matches('[scroll-border][show]'));
  }

  setup(async () => {
    document.body.innerHTML =
        '<div id="container"><div id="content"></div></div>';
    container = document.querySelector('#container');
    container.style.height = '100px';
    container.style.overflow = 'auto';
    content = document.querySelector('#content');
    content.style.height = '200px';
    observer = createScrollBorders(container);
    top = document.body.firstElementChild;
    bottom = document.body.lastElementChild;
    await waitAfterNextRender();
  });

  teardown(() => {
    observer.disconnect();
  });

  test('bottom border show when more content available below', () => {
    assertHidden(top);
    assertShown(bottom);
  });

  test('borders shown when content available above and below', async () => {
    container.scrollTop = 10;
    await waitAfterNextRender();
    assertShown(top);
    assertShown(bottom);
  });

  test('bottom border hidden when no content available below', async () => {
    container.scrollTop = 200;
    await waitAfterNextRender();
    assertShown(top);
    assertHidden(bottom);
  });

  test('borders hidden when all content is shown', async () => {
    content.style.height = '100px';
    await waitAfterNextRender();
    assertHidden(top);
    assertHidden(bottom);
  });
});
