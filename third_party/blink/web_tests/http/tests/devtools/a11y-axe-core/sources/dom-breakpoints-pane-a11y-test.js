// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  await TestRunner.loadModule('axe_core_test_runner');
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.navigatePromise(
      '../../sources/debugger-breakpoints/resources/dom-breakpoints.html');

  TestRunner.addResult('Testing accessibility in the DOM breakpoints pane.');
  await UI.viewManager.showView('sources.domBreakpoints');
  const domBreakpointsPane =
      self.runtime.sharedInstance(BrowserDebugger.DOMBreakpointsSidebarPane);

  TestRunner.addResult('Setting DOM breakpoints.');
  const rootElement = await ElementsTestRunner.nodeWithIdPromise('rootElement');
  TestRunner.domDebuggerModel.setDOMBreakpoint(
      rootElement, SDK.DOMDebuggerModel.DOMBreakpoint.Type.SubtreeModified);

  const hostElement = await ElementsTestRunner.nodeWithIdPromise('hostElement');
  const breakpoint = TestRunner.domDebuggerModel.setDOMBreakpoint(
      hostElement, SDK.DOMDebuggerModel.DOMBreakpoint.Type.NodeRemoved);
  TestRunner.domDebuggerModel.toggleDOMBreakpoint(breakpoint, false);

  TestRunner.addResult(`DOM breakpoints pane text content: ${domBreakpointsPane.contentElement.deepTextContent()}`);
  TestRunner.addResult(
      'Running the axe-core linter on the DOM breakpoints pane.');
  await AxeCoreTestRunner.runValidation(domBreakpointsPane.contentElement);

  TestRunner.completeTest();
})();
