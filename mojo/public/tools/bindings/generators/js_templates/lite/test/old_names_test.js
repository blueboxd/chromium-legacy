// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(() => {
  async function testFunction() {
    /** @type {oldNameTest.mojom.TestPageHandlerProxy} */
    let proxy = oldNameTest.mojom.TestPageHandler.getProxy()

    // Type infers {?{values: !Array<!string>}} from Promise return type.
    let result = await proxy.method1(' ', 5);

    /** @type {Array<string>} */
    let values = result.values;

    /** @type {oldNameTest.mojom.TestStruct} */
    let testStruct = result.ts
  }

  /** @implements {oldNameTest.mojom.TestPageInterface} */
  class TestPageImpl {
    /** @override */
    onEvent1(s) {
      /** @type {oldNameTest.mojom.TestStruct} */ let t = s;
      /** @type {string} */ let id = t.id;
      /** @type {string|undefined} */ let title = t.title;
      /** @type {oldNameTest.mojom.TestEnum} */ let enumValue = t.enums[0];

      /** @type {string} */ let numberToStringMapValue = t.numberToStringMap[5];

      /** @type {oldNameTest.mojom.Message} */
      let messageToMessageArrayValue =
        t.messageToArrayMap.get({message: 'asdf'})[0];

      /** @type {oldNameTest.mojom.TestEnum} */ let enumToMapMapValue =
        t.enumToMapMap[oldNameTest.mojom.TestEnum.FIRST]
                      [oldNameTest.mojom.TestEnum.SECOND];
      /** @type {oldNameTest.mojom.TestPageInterface} */
      let handler = t.numberToInterfaceProxyMap[3];
      handler.onEvent1(t);
    }
  }
})();
