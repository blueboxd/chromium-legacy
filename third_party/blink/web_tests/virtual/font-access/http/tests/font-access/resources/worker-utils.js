'use strict';

function promiseWorkerReady(t, worker) {
  return new Promise((resolve, reject) => {
    let timeout;

    const readyFunc = t.step_func(event => {
      if (event.data.type != 'ready') {
        reject(event);
      }
      window.clearTimeout(timeout);
      resolve();
    });

    // Setting a timeout because the the worker
    // may not be ready to listen to the postMessage.
    timeout = t.step_timeout(() => {
      if ('port' in worker) {
        if (!worker.port.onmessage) {
          worker.port.onmessage = readyFunc;
        }
        worker.port.postMessage({action: 'ping'});
      } else {
        if (!worker.onmessage) {
          worker.onmessage = readyFunc;
        }
        worker.postMessage({action: 'ping'});
      }
    }, 10);
  });
}

function promiseHandleMessage(t, worker) {
  return new Promise((resolve, reject) => {
    const handler = t.step_func(event => {
      if (event.data.type === 'ready') {
        // Ignore: may received duplicate readiness messages.
        return;
      }

      if (event.data.type === 'error') {
        reject(event);
      }

      resolve(event);
    });

    if ('port' in worker) {
      worker.port.onmessage = handler;
    } else {
      worker.onmessage = handler;
    }
  });
}
