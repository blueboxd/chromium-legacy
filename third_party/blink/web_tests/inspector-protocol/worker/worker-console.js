(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests how console messages from worker get into page's console once worker is destroyed.`);

  await session.evaluate(`
    let worker;

    // This function returns a promise, which, if awaited (e.g. with
    // await session.evaluateAsync("startWorker()") ensures that the
    // worker has been created. This works by worker-console-worker.js posting
    // a message upon which worker.onmessage resolves the promise.
    function startWorker() {
      return new Promise(resolve => {
        worker = new Worker(
            '${testRunner.url('../resources/worker-console-worker.js')}');
        worker.onmessage = () => { resolve(); }
      });
    }

    // Similar to the above method, this method returns a promise such that
    // awaiting it ensures that the worker has posted a message back to the
    // page. In this way, we know that worker-console-worker.js gets a chance
    // to call console.log.
    // We can terminate the worker after awaiting the promise returned by this
    // method, and then observe that the console.log events were registered
    // anyway. This is much of the point of this test.
    function postToWorker(message) {
      return new Promise(resolve => {
        worker.onmessage = () => { resolve(); };
        worker.postMessage(message);
      });
    }
  `);

  await dp.Log.enable();
  dp.Log.onEntryAdded((event) => {
    testRunner.log('<- Log from page: ' + event.params.entry.text);
  });

  function postToWorker(message) {
    testRunner.log('-> Posting to worker: ' + message);
    return session.evaluateAsync('postToWorker("' + message + '")');
  }

  {
    testRunner.log(
        "\n=== console.log event won't get lost despite worker.terminate. ===");
    testRunner.log('Starting worker');
    await session.evaluateAsync('startWorker()');
    await postToWorker('message0 (posted after starting worker)');
    testRunner.log('Terminating worker');
    await session.evaluate('worker.terminate()');
    // The key part of this test is that its expectation contains
    // "<- Log from page: message1", even though we terminated the
    // worker without awaiting the log event (postToWorker only ensures
    // that the message was echoed back to the page).
  }

  {
    testRunner.log(
        '\n=== Scenario with autoattach enabled and stopped. ===');
    testRunner.log('Starting worker');
    await session.evaluateAsync('startWorker()');
    await postToWorker('message1');

    // Now we call Target.setAutoAttach, which will immediately generate
    // an event that we're attached; which we receive below to create the
    // childSession instance.
    testRunner.log('Starting autoattach');
    dp.Target.setAutoAttach({
      autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
    const childSession = session.createChild(
        (await dp.Target.onceAttachedToTarget()).params.sessionId);
    childSession.protocol.Runtime.onConsoleAPICalled((event) => {
      testRunner.log('<-- Console API from worker: ' +
                     event.params.args[0].value);
    });
    testRunner.log('child session for worker created');

    testRunner.log('Sending Runtime.enable to worker');
    childSession.protocol.Runtime.enable();

    await postToWorker('message2 (posted after runtime enabled)');
    await postToWorker('throw1 (posted after runtime enabled; ' +
                       'yields exception in worker)');

    // This unregisters the child session, so we stop getting the console API
    // calls, but still receive the log messages for the page.
    testRunner.log('Stopping autoattach');
    await dp.Target.setAutoAttach({autoAttach: false,
                                   waitForDebuggerOnStart: false});
    await postToWorker('message3 (posted after auto-attach)');
    testRunner.log('Terminating worker');
    await session.evaluate('worker.terminate()');
  }

  {
    testRunner.log(
        '\n=== Scenario with autoattach from the get-go. ===');
    // This time we start the worker only after Target.setAutoAttach, so
    // we may await the autoattach response.
    testRunner.log('Starting autoattach');
    await dp.Target.setAutoAttach({
      autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

    testRunner.log('Starting worker');
    session.evaluate('startWorker()');
    const childSession = session.createChild(
        (await dp.Target.onceAttachedToTarget()).params.sessionId);
    childSession.protocol.Runtime.onConsoleAPICalled((event) => {
      testRunner.log('<-- Console API from worker: ' +
                     event.params.args[0].value);
    });
    testRunner.log('child session for worker created');

    // Here, we test the behavior of posting before / after Runtime.enable.
    await postToWorker(
        "message4 (posted before worker's runtime agent enabled)");
    testRunner.log('Sending Runtime.enable to worker');
    childSession.protocol.Runtime.enable();
    await postToWorker(
        "message5 (posted after worker's runtime agent enabled)");
    testRunner.log('Terminating worker');
    await session.evaluate('worker.terminate()');
  }

  {
    testRunner.log(
        '\n=== New worker, with auto-attach still enabled. ===');
    testRunner.log('Starting worker');
    session.evaluate('startWorker()');
    const childSession = session.createChild(
        (await dp.Target.onceAttachedToTarget()).params.sessionId);
    childSession.protocol.Runtime.onConsoleAPICalled((event) => {
      testRunner.log('<-- Console API from worker: ' +
                     event.params.args[0].value);
    });
    testRunner.log('child session for worker created');

    await postToWorker('message6 (posted just before worker termination)');

    testRunner.log('Terminating worker');
    await session.evaluate('worker.terminate()');

    testRunner.log('Stopping autoattach');
    await dp.Target.setAutoAttach({autoAttach: false,
                                   waitForDebuggerOnStart: false});
  }
  testRunner.completeTest();
})
