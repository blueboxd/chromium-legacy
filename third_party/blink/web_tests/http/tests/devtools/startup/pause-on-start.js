// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  Root.Runtime._queryParamsObject.set('panel', 'sources');
  await TestRunner.setupStartupTest('resources/pause-on-start.html');
  TestRunner.addResult(
      `Tests that tools pause on start.\n`);
  SDK.targetManager.addModelListener(
      SDK.DebuggerModel, SDK.DebuggerModel.Events.DebuggerPaused, (event) => {
    const name = event.data.debuggerPausedDetails().callFrames[0].functionName;
    TestRunner.addResult(name);
    TestRunner.completeTest();
  });
})();
