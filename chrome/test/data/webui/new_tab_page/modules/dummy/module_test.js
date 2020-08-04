// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$, dummyDescriptor} from 'chrome://new-tab-page/new_tab_page.js';

suite('NewTabPageModulesDummyModuleTest', () => {
  test('creates module', async () => {
    // Act.
    const module = await dummyDescriptor.create();
    document.body.append(module);

    // Assert.
    assertEquals('chrome://new-tab-page/', $$(module, 'a').href);
  });
});
