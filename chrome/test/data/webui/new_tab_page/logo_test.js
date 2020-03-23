// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/logo.js';

import {BrowserProxy} from 'chrome://new-tab-page/browser_proxy.js';
import {assertNotStyle, assertStyle, createTestProxy} from 'chrome://test/new_tab_page/test_support.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

suite('NewTabPageLogoTest', () => {
  /**
   * @implements {BrowserProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  async function createLogo(doodle) {
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
    const img = logo.shadowRoot.querySelector('img');
    const iframe = logo.shadowRoot.querySelector('ntp-untrusted-iframe');
    assertEquals(img.src, 'data:foo');
    assertNotStyle(img, 'display', 'none');
    assertStyle(iframe, 'display', 'none');
  });

  test('setting interactive doodle shows iframe', async () => {
    // Act.
    const logo = await createLogo({content: {url: {url: 'https://foo.com'}}});

    // Assert.
    const iframe = logo.shadowRoot.querySelector('ntp-untrusted-iframe');
    const img = logo.shadowRoot.querySelector('img');
    assertEquals(iframe.path, 'iframe?https://foo.com');
    assertNotStyle(iframe, 'display', 'none');
    assertStyle(img, 'display', 'none');
  });
});
