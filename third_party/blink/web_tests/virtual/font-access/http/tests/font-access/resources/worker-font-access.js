'use strict';

if (self.postMessage) {
  self.onmessage = handleMessage;
  postMessage('ready');
} else {
  // Makes Shared workers behave more like Dedicated workers.
  self.onconnect = (event) => {
    self.postMessage = (msg) => {
      event.ports[0].postMessage(msg);
    };
    event.ports[0].onmessage = handleMessage;
    postMessage('ready');
  };
}

async function handleMessage(event) {
  if (event.data == "query") {
    try {
      let availableFonts = [];
      for await (let f of navigator.fonts.query()) {
        availableFonts.push({
          postscriptName: f.postscriptName,
          fullName: f.fullName,
          family: f.family,
        });
      }
      postMessage(availableFonts);
    } catch(e) {
      postMessage(`Error: ${e}`);
    }
  } else {
    postMessage("FAILURE: Received unknown message: " + event.data);
  }
}
