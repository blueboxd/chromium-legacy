// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ModuleRegistry} from 'chrome://new-tab-page/new_tab_page.js';

suite('NewTabPageModulesModuleRegistryTest', () => {
  test('instantiates modules', async () => {
    // Arrange.
    const fooModule = document.createElement('div');
    const bazModule = document.createElement('div');
    const registry = new ModuleRegistry([
      {
        id: 'foo',
        create: () => Promise.resolve(fooModule),
      },
      {
        id: 'bar',
        create: () => null,
      },
      {
        id: 'baz',
        create: () => Promise.resolve(bazModule),
      }
    ]);
    const container = document.createElement('div');

    // Act.
    await registry.instantiateModules(container);

    // Assert.
    assertEquals(2, container.children.length);
    assertDeepEquals(fooModule, container.children[0]);
    assertDeepEquals(bazModule, container.children[1]);
  });
});
