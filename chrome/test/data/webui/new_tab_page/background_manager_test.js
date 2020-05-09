// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BackgroundManager, BrowserProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {createTestProxy} from './test_support.js';

suite('NewTabPageBackgroundManagerTest', () => {
  /** @type {!BackgroundManager} */
  let backgroundManager;

  /** @type {Element} */
  let backgroundImage;

  /**
   * @implements {BrowserProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  setup(() => {
    PolymerTest.clearBody();

    testProxy = createTestProxy();
    BrowserProxy.instance_ = testProxy;

    backgroundImage = document.createElement('div');
    backgroundImage.id = 'backgroundImage';
    document.body.appendChild(backgroundImage);

    backgroundManager = new BackgroundManager();
  });

  test('showing background image sets attribute', () => {
    // Act.
    backgroundManager.setShowBackgroundImage(true);

    // Assert.
    assertTrue(document.body.hasAttribute('show-background-image'));
  });

  test('hiding background image removes attribute', () => {
    // Arrange.
    backgroundManager.setShowBackgroundImage(true);

    // Act.
    backgroundManager.setShowBackgroundImage(false);

    // Assert.
    assertFalse(document.body.hasAttribute('show-background-image'));
  });

  test('setting url updates src', () => {
    // Act.
    backgroundManager.setBackgroundImageUrl('https://example.com');

    // Assert.
    assertEquals('https://example.com', backgroundImage.src);
  });

  test('receiving load time resolves promise', async () => {
    // Arrange.
    backgroundManager.setBackgroundImageUrl('https://example.com');

    // Act.
    const promise = backgroundManager.getBackgroundImageLoadTime();
    window.dispatchEvent(new MessageEvent('message', {
      data: {
        frameType: 'background-image',
        messageType: 'loaded',
        url: 'https://example.com',
        time: 123,
      }
    }));

    // Assert.
    assertEquals(123, await promise);
  });

  test('receiving load time resolves multiple promises', async () => {
    // Arrange.
    backgroundManager.setBackgroundImageUrl('https://example.com');

    // Act.
    const promises = [
      backgroundManager.getBackgroundImageLoadTime(),
      backgroundManager.getBackgroundImageLoadTime(),
      backgroundManager.getBackgroundImageLoadTime(),
    ];
    window.dispatchEvent(new MessageEvent('message', {
      data: {
        frameType: 'background-image',
        messageType: 'loaded',
        url: 'https://example.com',
        time: 123,
      }
    }));

    // Assert.
    const values = await Promise.all(promises);
    values.forEach((value) => {
      assertEquals(123, value);
    });
  });

  test('setting new url rejects promise', async () => {
    // Arrange.
    backgroundManager.setBackgroundImageUrl('https://example.com');

    // Act.
    const promise = backgroundManager.getBackgroundImageLoadTime();
    backgroundManager.setBackgroundImageUrl('https://other.com');

    // Assert.
    return promise.then(
        assertNotReached,
        () => {
            // Success. Nothing to do here.
        });
  });
});
