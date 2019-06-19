// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Add additional setup steps to the object from webvr_e2e.js if it exists.
if (typeof initializationSteps !== 'undefined') {
  initializationSteps['magicWindowStarted'] = false;
} else {
  // Create here if it doesn't exist so we can access it later without checking
  // if it's defined.
  var initializationSteps = {};
}

var webglCanvas = document.getElementById('webgl-canvas');
var glAttribs = {
  alpha: false,
  xrCompatible: true,
};
var gl = null;
var onMagicWindowXRFrameCallback = null;
var onImmersiveXRFrameCallback = null;
var onSessionStartedCallback = null;
var onPoseCallback = null;
var shouldSubmitFrame = true;
var hasPresentedFrame = false;
var arSessionRequestWouldTriggerPermissionPrompt = null;

var sessionTypes = Object.freeze({
  IMMERSIVE: 1,
  AR: 2,
  MAGIC_WINDOW: 3
});
var sessionTypeToRequest = sessionTypes.IMMERSIVE;

var referenceSpaceMap = {
  [sessionTypes.IMMERSIVE]: 'local',
  [sessionTypes.MAGIC_WINDOW]: 'viewer',
  [sessionTypes.AR]: 'viewer'
}

class SessionInfo {
  constructor() {
    this.session = null;
    this.frameOfRef = null;
    this.error = null;
  }

  get currentSession() {
    return this.session;
  }

  get currentRefSpace() {
    return this.frameOfRef;
  }

  set currentSession(session) {
    this.session = session;
  }

  set currentRefSpace(frameOfRef) {
    this.frameOfRef = frameOfRef;
  }

  clearSession() {
    this.session = null;
    this.frameOfRef = null;
    this.error = null;
  }
}

var sessionInfos = {}
sessionInfos[sessionTypes.IMMERSIVE] = new SessionInfo();
sessionInfos[sessionTypes.AR] = new SessionInfo();
sessionInfos[sessionTypes.MAGIC_WINDOW] = new SessionInfo();

function getSessionType(session) {
  if (session.mode == 'immersive-vr') {
    return sessionTypes.IMMERSIVE;
  } else if (session.mode == 'immersive-ar') {
    return sessionTypes.AR;
  } else {
    return sessionTypes.MAGIC_WINDOW;
  }
}

function onRequestSession() {
  switch (sessionTypeToRequest) {
    case sessionTypes.IMMERSIVE:
      console.info('Requesting immersive VR session');
      navigator.xr.requestSession('immersive-vr').then( (session) => {
        session.mode = 'immersive-vr';
        console.info('Immersive VR session request succeeded');
        sessionInfos[sessionTypes.IMMERSIVE].currentSession = session;
        onSessionStarted(session);
      }, (error) => {
        sessionInfos[sessionTypes.IMMERSIVE].error = error;
        console.info('Immersive VR session request rejected with: ' + error);
      });
      break;
    case sessionTypes.AR:
      console.info('Requesting Immersive AR session');
      navigator.xr.requestSession('immersive-ar').then((session) => {
        session.mode = 'immersive-ar';
        console.info('Immersive AR session request succeeded');
        sessionInfos[sessionTypes.AR].currentSession = session;
        onSessionStarted(session);
      }, (error) => {
        sessionInfos[sessionTypes.AR].error = error;
        console.info('Immersive AR session request rejected with: ' + error);
     });
      break;
    default:
      throw 'Given unsupported WebXR session type enum ' + sessionTypeToRequest;
  }
}

function onSessionStarted(session) {
  session.addEventListener('end', onSessionEnded);
  // Initialize the WebGL context for use with XR if it hasn't been already
  if (!gl) {
    gl = webglCanvas.getContext('webgl', glAttribs);
    if (!gl) {
      throw 'Failed to get WebGL context';
    }
    gl.clearColor(0.0, 1.0, 0.0, 1.0);
    gl.enable(gl.DEPTH_TEST);
    gl.enable(gl.CULL_FACE);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
  }

  if (onSessionStartedCallback) {
    onSessionStartedCallback(session);
  }

  let sessionType = getSessionType(session);

  session.updateRenderState({ baseLayer: new XRWebGLLayer(session, gl, {
      compositionDisabled: sessionType == sessionTypes.MAGIC_WINDOW
    })
  });
  session.requestReferenceSpace(referenceSpaceMap[sessionType])
      .then( (refSpace) => {
        sessionInfos[sessionType].currentRefSpace = refSpace;
        session.requestAnimationFrame(onXRFrame);
      });
}

function onSessionEnded(event) {
  sessionInfos[getSessionType(event.session)].clearSession();
}

function onXRFrame(t, frame) {
  let session = frame.session;
  session.requestAnimationFrame(onXRFrame);

  let refSpace = sessionInfos[getSessionType(session)].currentRefSpace;
  let pose = frame.getViewerPose(refSpace);
  if (onPoseCallback) {
    onPoseCallback(pose);
  }

  // Exiting the rAF callback without dirtying the GL context is interpreted
  // as not submitting a frame
  if (!shouldSubmitFrame) {
    return;
  }

  gl.bindFramebuffer(gl.FRAMEBUFFER, session.renderState.baseLayer.framebuffer);

  // If in an immersive session, set canvas to blue.
  // If in an AR session, just draw the camera.
  // Otherwise, red.
  switch (getSessionType(session)) {
    case sessionTypes.IMMERSIVE:
      gl.clearColor(0.0, 0.0, 1.0, 1.0);
      if (onImmersiveXRFrameCallback) {
        onImmersiveXRFrameCallback(session, frame, gl);
      }
      gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
      break;
    case sessionTypes.AR:
      // Do nothing for now
      break;
    default:
      if (onMagicWindowXRFrameCallback) {
        onMagicWindowXRFrameCallback(session, frame);
      }
      gl.clearColor(1.0, 0.0, 0.0, 1.0);
      gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
  }

  hasPresentedFrame = true;
}

function requestMagicWindowSession() {
  // Set up an inline session (magic window) drawing into the full screen canvas
  // on the page
  navigator.xr.requestSession('inline')
  .then((session) => {
    session.mode = 'inline';
    sessionInfos[sessionTypes.MAGIC_WINDOW].currentSession = session;
    onSessionStarted(session);
  })
  .then( () => {
    initializationSteps['magicWindowStarted'] = true;
  });
}

if (navigator.xr) {

  // WebXR for VR tests want an inline session to be automatically
  // created on page load to reduce the amount of boilerplate code necessary.
  // However, doing so during AR tests currently fails due to AR sessions
  // always requiring a user gesture. So, allow a page to set a variable
  // before loading this JavaScript file if they wish to skip the automatic
  // inline session creation.
  if (typeof shouldAutoCreateNonImmersiveSession === 'undefined'
      || shouldAutoCreateNonImmersiveSession === true) {

    // Separate if statement to keep the logic around setting initialization
    // steps cleaner.
    if (typeof shouldDeferNonImmersiveSessionCreation === 'undefined'
      || shouldDeferNonImmersiveSessionCreation === false) {
      requestMagicWindowSession();
    }
  } else {
    initializationSteps['magicWindowStarted'] = true;
  }

} else {
  initializationSteps['magicWindowStarted'] = true;
}

webglCanvas.onclick = onRequestSession;
