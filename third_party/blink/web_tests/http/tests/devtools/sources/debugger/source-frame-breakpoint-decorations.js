// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Checks that JavaScriptSourceFrame show breakpoints correctly\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('resources/edit-me-breakpoints.js');

  async function runAsyncBreakpointActionAndDumpDecorations(sourceFrame, action) {
    const waitPromise = SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame);
    await action();
    await waitPromise;
    SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);
  }

  Bindings.breakpointManager._storage._breakpoints = new Map();
  SourcesTestRunner.runDebuggerTestSuite([
    function testAddRemoveBreakpoint(next) {
      var javaScriptSourceFrame;
      SourcesTestRunner.showScriptSource('edit-me-breakpoints.js', addBreakpoint);

      function addBreakpoint(sourceFrame) {
        javaScriptSourceFrame = sourceFrame;
        TestRunner.addResult('Setting breakpoint');
        runAsyncBreakpointActionAndDumpDecorations(javaScriptSourceFrame, () =>
          SourcesTestRunner.createNewBreakpoint(javaScriptSourceFrame, 2, '', true)
        ).then(removeBreakpoint);
      }

      function removeBreakpoint() {
        TestRunner.addResult('Toggle breakpoint');
        runAsyncBreakpointActionAndDumpDecorations(javaScriptSourceFrame, () =>
          SourcesTestRunner.toggleBreakpoint(javaScriptSourceFrame, 2)
        ).then(next);
      }
    },

    function testTwoBreakpointsResolvedInOneLine(next) {
      var javaScriptSourceFrame;
      SourcesTestRunner.showScriptSource('edit-me-breakpoints.js', addBreakpoint);

      function addBreakpoint(sourceFrame) {
        javaScriptSourceFrame = sourceFrame;
        TestRunner.addResult('Setting breakpoint');
        SourcesTestRunner.createNewBreakpoint(javaScriptSourceFrame, 2, '', true)
            .then(() => SourcesTestRunner.waitBreakpointSidebarPane(true))
            .then(() => runAsyncBreakpointActionAndDumpDecorations(javaScriptSourceFrame, () =>
              SourcesTestRunner.createNewBreakpoint(javaScriptSourceFrame, 2, 'true', true)
            )).then(removeBreakpoint);
      }

      function removeBreakpoint() {
        TestRunner.addResult('Toggle breakpoint');
        runAsyncBreakpointActionAndDumpDecorations(javaScriptSourceFrame, () =>
          SourcesTestRunner.toggleBreakpoint(javaScriptSourceFrame, 2)
        ).then(next);
      }
    },

    function testDecorationInGutter(next) {
      var javaScriptSourceFrame;
      SourcesTestRunner.showScriptSource('edit-me-breakpoints.js', addRegularDisabled);

      function addRegularDisabled(sourceFrame) {
        javaScriptSourceFrame = sourceFrame;
        TestRunner.addResult('Adding regular disabled breakpoint');
        runAsyncBreakpointActionAndDumpDecorations(javaScriptSourceFrame, () =>
          SourcesTestRunner.createNewBreakpoint(javaScriptSourceFrame, 2, '', false)
        ).then(addConditionalDisabled);
      }

      function addConditionalDisabled() {
        TestRunner.addResult('Adding conditional disabled breakpoint');
        runAsyncBreakpointActionAndDumpDecorations(javaScriptSourceFrame, () =>
          SourcesTestRunner.createNewBreakpoint(javaScriptSourceFrame, 2, 'true', false)
        ).then(addRegularEnabled);
      }

      async function addRegularEnabled() {
        TestRunner.addResult('Adding regular enabled breakpoint');
        runAsyncBreakpointActionAndDumpDecorations(javaScriptSourceFrame, () =>
          SourcesTestRunner.createNewBreakpoint(javaScriptSourceFrame, 2, '', true)
        ).then(addConditionalEnabled);
      }

      function addConditionalEnabled() {
        TestRunner.addResult('Adding conditional enabled breakpoint');
        runAsyncBreakpointActionAndDumpDecorations(javaScriptSourceFrame, () =>
          SourcesTestRunner.createNewBreakpoint(javaScriptSourceFrame, 2, 'true', true)
        ).then(disableAll);
      }

      function disableAll() {
        TestRunner.addResult('Disable breakpoints');
        runAsyncBreakpointActionAndDumpDecorations(javaScriptSourceFrame, () =>
          SourcesTestRunner.toggleBreakpoint(javaScriptSourceFrame, 2, true)
        ).then(enabledAll);
      }

      function enabledAll() {
        TestRunner.addResult('Enable breakpoints');
        runAsyncBreakpointActionAndDumpDecorations(javaScriptSourceFrame, () =>
          SourcesTestRunner.toggleBreakpoint(javaScriptSourceFrame, 2, true)
        ).then(removeAll);
      }

      function removeAll() {
        TestRunner.addResult('Remove breakpoints');
        runAsyncBreakpointActionAndDumpDecorations(javaScriptSourceFrame, () =>
          SourcesTestRunner.toggleBreakpoint(javaScriptSourceFrame, 2, false)
        ).then(next);
      }
    }
  ]);
})();
