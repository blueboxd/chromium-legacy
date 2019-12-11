// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Need to import that otherwise we get an "Uncaught ReferenceError: mojo is not
// defined" error.
// TODO(tiborg): Refactor so that this import is not needed anymore.
import 'chrome://new-tab-page/browser_proxy.js';
import 'chrome://new-tab-page/customize_dialog.js';

import {flushTasks} from 'chrome://test/test_util.m.js';

suite('NewTabPageCustomizeDialogTest', () => {
  /** @type {!CustomizeDialogElement} */
  let customizeDialog;

  setup(() => {
    PolymerTest.clearBody();

    customizeDialog = document.createElement('ntp-customize-dialog');
    document.body.appendChild(customizeDialog);
  });

  test('setting colors shows color tiles', async () => {
    // Act.
    customizeDialog.colors_ = [
      {
        label: 'color_0',
        icon: 'data_0',
      },
      {
        label: 'color_1',
        icon: 'data_1',
      },
    ];
    await flushTasks();

    // Assert.
    const colorTiles =
        Array.from(customizeDialog.shadowRoot.querySelectorAll('.tile'));
    assertEquals(colorTiles.length, 2);
    assertEquals(colorTiles[0].getAttribute('title'), 'color_0');
    assertEquals(colorTiles[0].style['background-image'], 'url("data_0")');
    assertEquals(colorTiles[1].getAttribute('title'), 'color_1');
    assertEquals(colorTiles[1].style['background-image'], 'url("data_1")');
  });
});
