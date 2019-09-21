// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that the console works correctly with portals`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');

  await TestRunner.navigatePromise('resources/portal_host.html');

  async function setContextLabel(target, label) {
    var runtimeModel = target.model(SDK.RuntimeModel);
    await TestRunner.waitForExecutionContext(runtimeModel);
    runtimeModel.executionContexts()[0].setLabel(label);
  }

  var targets = SDK.targetManager.targets();
  TestRunner.assertEquals(2, targets.length);

  TestRunner.runTestSuite([
    async function testMainConsole(next) {
      ConsoleTestRunner.selectMainExecutionContext();
      ConsoleTestRunner.evaluateInConsoleAndDump("!!window.portalHost", next);
    },

    async function testPortalConsole() {
      await setContextLabel(targets[1], "portal");
      ConsoleTestRunner.changeExecutionContext("portal");
      ConsoleTestRunner.evaluateInConsoleAndDump("!!window.portalHost",
                                                 TestRunner.completeTest, true);
    }
  ]);
})();
