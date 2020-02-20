// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/customize_backgrounds.js';

import {BrowserProxy} from 'chrome://new-tab-page/browser_proxy.js';
import {createTestProxy} from 'chrome://test/new_tab_page/test_support.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

suite('NewTabPageCustomizeBackgroundsTest', () => {
  /** @type {TestProxy} */
  let testProxy;

  setup(() => {
    PolymerTest.clearBody();

    testProxy = createTestProxy();
    BrowserProxy.instance_ = testProxy;
  });

  test('creating element shows background collection tiles', async () => {
    // Arrange.
    const collections = [
      {
        label: 'collection_0',
        previewImageUrl: {url: 'https://example.com/image_0.jpg'},
      },
      {
        label: 'collection_1',
        previewImageUrl: {url: 'https://example.com/image_1.jpg'},
      },
    ];
    const getBackgroundCollectionsCalled =
        testProxy.handler.whenCalled('getBackgroundCollections');
    testProxy.handler.setResultFor('getBackgroundCollections', Promise.resolve({
      collections: collections,
    }));

    // Act.
    const customizeBackgrounds =
        document.createElement('ntp-customize-backgrounds');
    document.body.appendChild(customizeBackgrounds);
    await getBackgroundCollectionsCalled;
    await flushTasks();

    // Assert.
    const tiles = customizeBackgrounds.shadowRoot.querySelectorAll('.tile');
    assertEquals(tiles.length, 2);
    assertEquals(tiles[0].getAttribute('title'), 'collection_0');
    assertEquals(tiles[1].getAttribute('title'), 'collection_1');
    assertEquals(
        tiles[0].querySelector('.image').textContent.trim(),
        'https://example.com/image_0.jpg');
    assertEquals(
        tiles[1].querySelector('.image').textContent.trim(),
        'https://example.com/image_1.jpg');
  });
});
