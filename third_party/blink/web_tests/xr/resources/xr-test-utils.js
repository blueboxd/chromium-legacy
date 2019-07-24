// func: A function that takes a session and optionally a test object and
// performs tests. If func returns a promise, test will only pass if the promise
// resolves.
function xr_session_promise_test(
    name, func, deviceOptions, sessionMode, sessionInit, properties) {
  let testSession = null;
  let sessionObjects = {};

  const webglCanvas = document.getElementsByTagName('canvas')[0];
  // We can't use assert_true here because it causes the wpt testharness to treat
  // this as a test page and not as a test.
  if (!webglCanvas) {
    promise_test(async (t) => {
      Promise.reject('xr_session_promise_test requires a canvas on the page!');
    }, name, properties);
  }
  let gl = webglCanvas.getContext('webgl', {alpha: false, antialias: false});
  sessionObjects.gl = gl;
  promise_test((t) => {
    let fakeDeviceController;

    // Ensure that any pending sessions are ended and devices are
    // disconnected when done. This needs to use a cleanup function to
    // ensure proper sequencing. If this were done in a .then() for the
    // success case, a test that expected failure would already be marked
    // done at the time that runs, and the shutdown would interfere with
    // the next test which may have started already.
    t.add_cleanup(async() => {
      if (testSession) {
        // TODO(bajones): Throwing an error when a session is
        // already ended is not defined by the spec. This
        // should be defined or removed.
        await testSession.end().catch(() => {});
      }

      await navigator.xr.test.disconnectAllDevices();
    });

    return navigator.xr.test.simulateDeviceConnection(deviceOptions)
        .then((controller) => {
          fakeDeviceController = controller;
          if (gl) {
            return gl.makeXRCompatible().then(
                () => Promise.resolve());
          } else {
            return Promise.resolve();
          }
        })
        .then(() => new Promise((resolve, reject) => {
            // Perform the session request in a user gesture.
            navigator.xr.test.simulateUserActivation(() => {
              navigator.xr.requestSession(sessionMode, sessionInit || {})
                  .then((session) => {
                    testSession = session;
                    session.mode = sessionMode;
                    let glLayer = new XRWebGLLayer(session, gl, {
                      compositionDisabled: session.mode == 'inline'
                    });
                    sessionObjects.glLayer = glLayer;
                    // Session must have a baseLayer or frame requests
                    // will be ignored.
                    session.updateRenderState({
                        baseLayer: glLayer
                    });
                    resolve(func(session, fakeDeviceController, t, sessionObjects));
                  })
                  .catch((err) => {
                    reject('Session with params ' +
                      JSON.stringify(sessionMode) +
                        ' was rejected on device ' +
                        JSON.stringify(fakeDeviceInit) +
                        ' with error: ' + err);
                  });
            });
        }));
  }, name, properties);
}
