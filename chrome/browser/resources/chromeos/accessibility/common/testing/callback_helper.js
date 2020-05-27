// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Creates wrappers for callbacks and calls testDone() when all callbacks
 * have been invoked.
 * @param {testing.Test} fixture
 */
function CallbackHelper(fixture) {
  /** @type {Object} fixture */
  this.fixture_ = fixture;
  /** @type {number} */
  this.pendingCallbacks_ = 0;
}

CallbackHelper.prototype = {
  /**
   * @param {Function=} opt_callback
   * @param {boolean=} opt_isAsync True if callback is async.
   * @return {Function}
   */
  wrap(opt_callback, opt_isAsync) {
    const callback = opt_callback || function() {};
    const savedArgs = new SaveMockArguments();
    let lastCall = null;
    const completionAction = callFunctionWithSavedArgs(savedArgs, function() {
      if (lastCall) {
        throw new Error('Called more than once, first call here: ' + lastCall);
      } else {
        lastCall = new Error().stack;
      }
      const result = callback.apply(this.fixture_, arguments);
      if (opt_isAsync) {
        if (!result) {
          throw new Error('Expected function to return a Promise.');
        }
        result.then(() => {
          CallbackHelper.testDone_();
        });
      } else {
        if (--this.pendingCallbacks_ <= 0) {
          CallbackHelper.testDone_();
        }
      }
    }.bind(this));
    // runAllActionsAsync catches exceptions and puts them in the test
    // framework's list of errors and fails the test if appropriate.
    const runAll = runAllActionsAsync(WhenTestDone.ASSERT, completionAction);
    ++this.pendingCallbacks_;
    return function() {
      savedArgs.arguments = Array.prototype.slice.call(arguments);
      runAll.invoke();
    };
  }
};

/**
 * @private
 */
CallbackHelper.testDone_ = this.testDone;
// Remove testDone for public use since directly using it conflicts with
// this callback helper.
delete this.testDone;
