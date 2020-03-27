// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/logo.js';

import {BrowserProxy} from 'chrome://new-tab-page/browser_proxy.js';
import {assertNotStyle, assertStyle, createTestProxy} from 'chrome://test/new_tab_page/test_support.js';
import {eventToPromise, flushTasks} from 'chrome://test/test_util.m.js';

suite('NewTabPageLogoTest', () => {
  /**
   * @implements {BrowserProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  async function createLogo(doodle = null) {
    testProxy.handler.setResultFor('getDoodle', Promise.resolve({
      doodle: doodle,
    }));
    const logo = document.createElement('ntp-logo');
    document.body.appendChild(logo);
    await flushTasks();
    return logo;
  }

  setup(() => {
    PolymerTest.clearBody();

    testProxy = createTestProxy();
    BrowserProxy.instance_ = testProxy;
  });

  test('setting static, animated doodle shows image', async () => {
    // Act.
    const logo = await createLogo({content: {image: 'data:foo'}});

    // Assert.
    assertNotStyle(logo.$.doodle, 'display', 'none');
    assertStyle(logo.$.logo, 'display', 'none');
    assertEquals(logo.$.image.src, 'data:foo');
    assertNotStyle(logo.$.image, 'display', 'none');
    assertStyle(logo.$.iframe, 'display', 'none');
  });

  test('setting interactive doodle shows iframe', async () => {
    // Act.
    const logo = await createLogo({content: {url: {url: 'https://foo.com'}}});

    // Assert.
    assertNotStyle(logo.$.doodle, 'display', 'none');
    assertStyle(logo.$.logo, 'display', 'none');
    assertEquals(logo.$.iframe.path, 'iframe?https://foo.com');
    assertNotStyle(logo.$.iframe, 'display', 'none');
    assertStyle(logo.$.image, 'display', 'none');
  });

  test('disallowing doodle shows logo', async () => {
    // Act.
    const logo = await await createLogo({content: {image: 'data:foo'}});
    logo.doodleAllowed = false;

    // Assert.
    assertNotStyle(logo.$.logo, 'display', 'none');
    assertStyle(logo.$.doodle, 'display', 'none');
  });

  test('before doodle loaded shows nothing', () => {
    // Act.
    testProxy.handler.setResultFor('getDoodle', new Promise(() => {}));
    const logo = document.createElement('ntp-logo');
    document.body.appendChild(logo);

    // Assert.
    assertStyle(logo.$.logo, 'display', 'none');
    assertStyle(logo.$.doodle, 'display', 'none');
  });

  test('unavailable doodle shows logo', async () => {
    // Act.
    const logo = await createLogo();

    // Assert.
    assertNotStyle(logo.$.logo, 'display', 'none');
    assertStyle(logo.$.doodle, 'display', 'none');
  });

  test('not setting-single colored shows multi-colored logo', async () => {
    // Act.
    const logo = await createLogo();

    // Assert.
    assertNotStyle(logo.$.multiColoredLogo, 'display', 'none');
    assertStyle(logo.$.singleColoredLogo, 'display', 'none');
  });

  test('setting single-colored shows single-colored logo', async () => {
    // Act.
    const logo = await createLogo();
    logo.singleColored = true;
    logo.style.setProperty('--ntp-logo-color', 'red');

    // Assert.
    assertNotStyle(logo.$.singleColoredLogo, 'display', 'none');
    assertStyle(logo.$.singleColoredLogo, 'background-color', 'rgb(255, 0, 0)');
    assertStyle(logo.$.multiColoredLogo, 'display', 'none');
  });

  test('receiving resize message resizes doodle', async () => {
    // Arrange.
    const logo = await createLogo({content: {url: {url: 'https://foo.com'}}});
    const transitionend = eventToPromise('transitionend', logo.$.iframe);

    // Act.
    window.postMessage(
        {
          cmd: 'resizeDoodle',
          duration: '500ms',
          height: '500px',
          width: '700px',
        },
        '*');
    await transitionend;

    // Assert.
    const transitionedProperties = window.getComputedStyle(logo.$.iframe)
                                       .getPropertyValue('transition-property')
                                       .trim()
                                       .split(',')
                                       .map(s => s.trim());
    assertStyle(logo.$.iframe, 'transition-duration', '0.5s');
    assertTrue(transitionedProperties.includes('height'));
    assertTrue(transitionedProperties.includes('width'));
    assertEquals(logo.$.iframe.offsetHeight, 500);
    assertEquals(logo.$.iframe.offsetWidth, 700);
    assertGE(logo.offsetHeight, 500);
    assertGE(logo.offsetWidth, 700);
  });

  test('receiving other message does not resize doodle', async () => {
    // Arrange.
    const logo = await createLogo({content: {url: {url: 'https://foo.com'}}});
    const height = logo.$.iframe.offsetHeight;
    const width = logo.$.iframe.offsetWidth;

    // Act.
    window.postMessage(
        {
          cmd: 'foo',
          duration: '500ms',
          height: '500px',
          width: '700px',
        },
        '*');
    await flushTasks();

    // Assert.
    assertEquals(logo.$.iframe.offsetHeight, height);
    assertEquals(logo.$.iframe.offsetWidth, width);
  });
});
