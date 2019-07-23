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

function perspectiveFromFieldOfView(fov, near, far) {
  let upTan = Math.tan(fov.upDegrees * Math.PI / 180.0);
  let downTan = Math.tan(fov.downDegrees * Math.PI / 180.0);
  let leftTan = Math.tan(fov.leftDegrees * Math.PI / 180.0);
  let rightTan = Math.tan(fov.rightDegrees * Math.PI / 180.0);
  let xScale = 2.0 / (leftTan + rightTan);
  let yScale = 2.0 / (upTan + downTan);
  let nf = 1.0 / (near - far);

  let out = new Float32Array(16);
  out[0] = xScale;
  out[1] = 0.0;
  out[2] = 0.0;
  out[3] = 0.0;
  out[4] = 0.0;
  out[5] = yScale;
  out[6] = 0.0;
  out[7] = 0.0;
  out[8] = -((leftTan - rightTan) * xScale * 0.5);
  out[9] = ((upTan - downTan) * yScale * 0.5);
  out[10] = (near + far) * nf;
  out[11] = -1.0;
  out[12] = 0.0;
  out[13] = 0.0;
  out[14] = (2.0 * far * near) * nf;
  out[15] = 0.0;

  return out;
}

function assert_points_approx_equal(actual, expected, epsilon = FLOAT_EPSILON, message = '') {
  if (expected == null && actual == null) {
    return;
  }

  assert_not_equals(expected, null);
  assert_not_equals(actual, null);

  let mismatched_component = null;
  for (const v of ['x', 'y', 'z', 'w']) {
    if (Math.abs(expected[v] - actual[v]) > epsilon) {
      mismatched_component = v;
      break;
    }
  }

  if (mismatched_component !== null) {
    let error_message = message ? message + '\n' : 'Point comparison failed.\n';
    error_message += ` Expected: {x: ${expected.x}, y: ${expected.y}, z: ${expected.z}, w: ${expected.w}}\n`;
    error_message += ` Actual: {x: ${actual.x}, y: ${actual.y}, z: ${actual.z}, w: ${actual.w}}\n`;
    error_message += ` Difference in component ${mismatched_component} exceeded the given epsilon.\n`;
    assert_approx_equals(actual[mismatched_component], expected[mismatched_component], epsilon, error_message);
  }
}

function assert_matrices_approx_equal(
    matA, matB, epsilon = FLOAT_EPSILON, message = '') {
  if (matA == null && matB == null) {
    return;
  }

  assert_not_equals(matA, null);
  assert_not_equals(matB, null);

  assert_equals(matA.length, 16);
  assert_equals(matB.length, 16);

  let mismatched_element = -1;
  for (let i = 0; i < 16; ++i) {
    if (Math.abs(matA[i] - matB[i]) > epsilon) {
      mismatched_element = i;
      break;
    }
  }

  if (mismatched_element > -1) {
    let error_message =
        message ? message + '\n' : 'Matrix comparison failed.\n';
    error_message += ' Difference in element ' + mismatched_element +
        ' exceeded the given epsilon.\n';

    error_message += ' Matrix A: [' + matA.join(',') + ']\n';
    error_message += ' Matrix B: [' + matB.join(',') + ']\n';

    assert_approx_equals(
        matA[mismatched_element], matB[mismatched_element], epsilon,
        error_message);
  }
}


function assert_matrices_significantly_not_equal(
    matA, matB, epsilon = FLOAT_EPSILON, message = '') {
  if (matA == null && matB == null) {
    return;
  }

  assert_not_equals(matA, null);
  assert_not_equals(matB, null);

  assert_equals(matA.length, 16);
  assert_equals(matB.length, 16);

  let mismatch = false;
  for (let i = 0; i < 16; ++i) {
    if (Math.abs(matA[i] - matB[i]) > epsilon) {
      mismatch = true;
      break;
    }
  }

  if (!mismatch) {
    let matA_str = '[';
    let matB_str = '[';
    for (let i = 0; i < 16; ++i) {
      matA_str += matA[i] + (i < 15 ? ', ' : '');
      matB_str += matB[i] + (i < 15 ? ', ' : '');
    }
    matA_str += ']';
    matB_str += ']';

    let error_message =
        message ? message + '\n' : 'Matrix comparison failed.\n';
    error_message +=
        ' No element exceeded the given epsilon ' + epsilon + '.\n';

    error_message += ' Matrix A: ' + matA_str + '\n';
    error_message += ' Matrix B: ' + matB_str + '\n';

    assert_unreached(error_message);
  }
}
