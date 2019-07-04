// func: A function that takes a session and optionally a test object and
// performs tests. If func returns a promise, test will only pass if the promise
// resolves.
function xr_session_promise_test(
    func, deviceOptions, sessionModes, name, properties) {
  if (document.getElementById('webgl-canvas') ||
      document.getElementById('webgl2-canvas')) {
    webglCanvasSetup();
  }

  promise_test((t) => {
    let fakeDeviceController;
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
          // Run the test with each of sessionModes from the array one
          // at a time.
          function nextSessionTest(i) {
            // Check if it's time to break the loop.
            if (i == sessionModes.length) {
              if (sessionModes.length == 0) {
                reject('No modes specified. Test did not run.');
              } else {
                resolve();
              }
              return;
            }

            // Perform the session request in a user gesture.
            runWithUserGesture(() => {
              let nextMode = sessionModes[i];
              let testSession = null;
              navigator.xr.requestSession(nextMode)
                  .then((session) => {
                    testSession = session;
                    testSession.mode = nextMode;
                    return func(session, t, fakeDeviceController);
                  })
                  .then(() => {
                    // End the session. Silence any errors generated if the
                    // session was already ended.
                    // TODO(bajones): Throwing an error when a session is
                    // already ended is not defined by the spec. This
                    // should be defined or removed.
                    testSession.end().catch(() => {});
                  })
                  .then(() => nextSessionTest(++i))
                  .catch((err) => {
                    reject(
                        `Test failed while running with the following XRSessionMode:
                        ${nextMode} ${err}`);
                  });
            });
          }

          nextSessionTest(0);
        }));
  }, name, properties);
}

let webglCanvas, gl;
function webglCanvasSetup() {
  let webgl2 = false;
  webglCanvas = document.getElementById('webgl-canvas');
  if (!webglCanvas) {
    webglCanvas = document.getElementById('webgl2-canvas');
    webgl2 = true;
  }
  let glAttributes = {
    alpha: false,
    antialias: false,
  };
  gl = webglCanvas.getContext(webgl2 ? 'webgl2' : 'webgl', glAttributes);
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

function runWithUserGesture(fn) {
  function thunk() {
    document.removeEventListener('keypress', thunk, false);
    fn()
  }
  document.addEventListener('keypress', thunk, false);
  eventSender.keyDown(' ', []);
}
