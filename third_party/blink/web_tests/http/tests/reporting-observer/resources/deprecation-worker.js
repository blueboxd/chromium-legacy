function run(port) {
  const observer = new ReportingObserver((reports) => {
    port.postMessage(reports.map(report => report.toJSON()));
  });
  observer.observe();

  try {
    // This API is deprecated. This throws an exception because of a bad
    // argument, but we don't care.
    Atomics.wake();
  } catch (e) {
  }
}

// For DedicatedWorker and ServiceWorker
self.addEventListener('message', (e) => run(e.data));

// For SharedWorker
self.addEventListener('connect', (e) => {
  e.ports[0].onmessage = (ev) => run(ev.data);
});
