'use strict';

if (self.postMessage) {
  self.onmessage = handleMessage;
} else {
  // Makes Shared workers behave more like Dedicated workers.
  self.onconnect = (event) => {
    self.postMessage = (msg) => {
      event.ports[0].postMessage(msg);
    };
    event.ports[0].onmessage = handleMessage;
  };
}

async function enumerate() {
  const output = [];
  for await (const f of navigator.fonts.query()) {
    output.push({
      postscriptName: f.postscriptName,
      fullName: f.fullName,
      family: f.family,
    });
  }
  return output;
}

async function getTables(options) {
  options = Object.assign({
    postscriptNameFilter: [],
    tables: [],
  }, options);

  const nameFilterSet = new Set(options.postscriptNameFilter);
  const iterator = navigator.fonts.query();
  const localFonts = [];
  for await (const f of navigator.fonts.query()) {
    if (nameFilterSet.size > 0) {
      if (!nameFilterSet.has(f.postscriptName)) {
        continue;
      }
    }
    localFonts.push(f);
  }

  let output = [];
  for (const f of localFonts) {
    const fontTableMap = await f.getTables(options.tables);
    output.push({
      postscriptName: f.postscriptName,
      fullName: f.fullName,
      family: f.family,
      tables: new Map(fontTableMap),  // fontTableMap is not cloneable.
    });
  }

  return output;
}

async function handleMessage(event) {
  try {
    switch(event.data.action) {
    case "ping":
      postMessage({type: "ready"});
      break;
    case "query":
      postMessage({type: "query", data: await enumerate()});
      break;
    case "getTables":
      postMessage({type: "getTables", data: await getTables(event.data.options)});
      break;
    default:
      postMessage({type: "error", data: "FAILURE: Received unknown message: " + event.data});
    }
  } catch(e) {
    postMessage({type: "error", data: `Error: ${e}`});
  }
}
