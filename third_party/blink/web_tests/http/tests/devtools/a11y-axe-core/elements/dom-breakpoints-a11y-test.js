// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function() {
  TestRunner.addResult(
      'Tests accessibility in DOM breakpoints using the axe-core linter.');
  await TestRunner.loadModule('axe_core_test_runner');
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await UI.viewManager.showView('elements.domBreakpoints');

  await TestRunner.navigatePromise(
      '../../sources/debugger-breakpoints/resources/dom-breakpoints.html');
  await ElementsTestRunner.nodeWithId('rootElement', domBpTest);

  async function domBpTest(node) {
    if (!node) {
      TestRunner.addResult('Could not locate node with id rootElement.');
      TestRunner.completeTest();
      return;
    }

    // Add Dom breakpoints and then test
    TestRunner.domDebuggerModel.setDOMBreakpoint(
        node, SDK.DOMDebuggerModel.DOMBreakpoint.Type.SubtreeModified);
    TestRunner.domDebuggerModel.setDOMBreakpoint(
        node, SDK.DOMDebuggerModel.DOMBreakpoint.Type.AttributeModified);
    TestRunner.addResult(
        'Test DOM breakpoint container with multiple breakpoints.');
    const view = 'elements.domBreakpoints';
    const widget = await UI.viewManager.view(view).widget();
    await AxeCoreTestRunner.runValidation(widget.element);

    TestRunner.completeTest();
  }
})();
