// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/customize_backgrounds.js';

import {BrowserProxy} from 'chrome://new-tab-page/browser_proxy.js';
import {assertNotStyle, assertStyle, createTestProxy} from 'chrome://test/new_tab_page/test_support.js';
import {flushTasks, isVisible} from 'chrome://test/test_util.m.js';

function createCollection(id = 0, label = '', url = '') {
  return {id: id, label: label, previewImageUrl: {url: url}};
}

suite('NewTabPageCustomizeBackgroundsTest', () => {
  /** @type {newTabPage.mojom.PageHandlerRemote} */
  let handler;

  async function createCustomizeBackgrounds() {
    const customizeBackgrounds =
        document.createElement('ntp-customize-backgrounds');
    document.body.appendChild(customizeBackgrounds);
    await handler.whenCalled('getBackgroundCollections');
    await flushTasks();
    return customizeBackgrounds;
  }

  setup(() => {
    PolymerTest.clearBody();

    const testProxy = createTestProxy();
    handler = testProxy.handler;
    handler.setResultFor('getBackgroundCollections', Promise.resolve({
      collections: [],
    }));
    handler.setResultFor('getBackgroundImages', Promise.resolve({
      images: [],
    }));
    BrowserProxy.instance_ = testProxy;
  });

  test('creating element shows background collection tiles', async () => {
    // Arrange.
    const collection = createCollection(0, 'col_0', 'https://col_0.jpg');
    handler.setResultFor('getBackgroundCollections', Promise.resolve({
      collections: [collection],
    }));

    // Act.
    const customizeBackgrounds = await createCustomizeBackgrounds();

    // Assert.
    assertTrue(isVisible(customizeBackgrounds.$.collections));
    assertStyle(customizeBackgrounds.$.images, 'display', 'none');
    const tiles =
        customizeBackgrounds.shadowRoot.querySelectorAll('#collections .tile');
    assertEquals(tiles.length, 1);
    assertEquals(tiles[0].getAttribute('title'), 'col_0');
    assertEquals(
        tiles[0].querySelector('.image').path,
        'background_image?https://col_0.jpg');
  });

  test('clicking collection selects collection', async function() {
    // Arrange.
    const collection = createCollection();
    handler.setResultFor('getBackgroundCollections', Promise.resolve({
      collections: [collection],
    }));
    const customizeBackgrounds = await createCustomizeBackgrounds();

    // Act.
    customizeBackgrounds.shadowRoot.querySelector('#collections .tile').click();

    // Assert.
    assertDeepEquals(customizeBackgrounds.selectedCollection, collection);
  });

  test('setting collection requests images', async function() {
    // Arrange.
    const customizeBackgrounds = await createCustomizeBackgrounds();

    // Act.
    customizeBackgrounds.selectedCollection = createCollection();

    // Assert.
    assertFalse(isVisible(customizeBackgrounds.$.collections));
    await handler.whenCalled('getBackgroundImages');
  });

  test('Loading images shows image tiles', async function() {
    // Arrange.
    const image = {
      label: 'image_0',
      previewImageUrl: {url: 'https://example.com/image.png'},
    };
    handler.setResultFor('getBackgroundImages', Promise.resolve({
      images: [image],
    }));
    const customizeBackgrounds = await createCustomizeBackgrounds();
    customizeBackgrounds.selectedCollection = createCollection(0);

    // Act.
    const id = await handler.whenCalled('getBackgroundImages');
    await flushTasks();

    // Assert.
    assertEquals(id, 0);
    assertFalse(isVisible(customizeBackgrounds.$.collections));
    assertTrue(isVisible(customizeBackgrounds.$.images));
    const tiles =
        customizeBackgrounds.shadowRoot.querySelectorAll('#images .tile');
    assertEquals(tiles.length, 1);
    assertEquals(
        tiles[0].querySelector('.image').path,
        'background_image?https://example.com/image.png');
  });

  test('Going back shows collections', async function() {
    // Arrange.
    const image = {
      label: 'image_0',
      previewImageUrl: {url: 'https://example.com/image.png'},
    };
    const customizeBackgrounds = await createCustomizeBackgrounds();
    handler.setResultFor('getBackgroundImages', Promise.resolve({
      images: [image],
    }));
    customizeBackgrounds.selectedCollection = createCollection();
    await flushTasks();

    // Act.
    customizeBackgrounds.selectedCollection = null;
    await flushTasks();

    // Assert.
    assertNotStyle(customizeBackgrounds.$.collections, 'display', 'none');
    assertStyle(customizeBackgrounds.$.images, 'display', 'none');
  });
});
