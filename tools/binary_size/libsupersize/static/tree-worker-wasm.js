// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

importScripts('./shared.js');
importScripts('./caspian_web.js');

const _PATH_SEP = '/';
const _NAMES_TO_FLAGS = Object.freeze({
  hot: _FLAGS.HOT,
  generated: _FLAGS.GENERATED_SOURCE,
  coverage: _FLAGS.COVERAGE,
  uncompressed: _FLAGS.UNCOMPRESSED,
});


/**
 * Wrapper around fetch for requesting the same resource multiple times.
 */
class DataFetcher {
  constructor(input) {
    /** @type {AbortController | null} */
    this._controller = null;
    this.setInput(input);
  }

  /**
   * Sets the input that describes what will be fetched. Also clears the cache.
   * @param {string | Request} input URL to the resource you want to fetch.
   */
  setInput(input) {
    if (typeof this._input === 'string' && this._input.startsWith('blob:')) {
      // Revoke the previous Blob url to prevent memory leaks
      URL.revokeObjectURL(this._input);
    }

    /** @type {Uint8Array | null} */
    this._cache = null;
    this._input = input;
  }

  /**
   * Starts a new request and aborts the previous one.
   * @param {string | Request} url
   */
  async fetch(url) {
    if (this._controller) this._controller.abort();
    this._controller = new AbortController();
    const headers = new Headers();
    headers.append('cache-control', 'no-cache');
    return fetch(url, {
      headers,
      credentials: 'same-origin',
      signal: this._controller.signal,
    });
  }

  /**
   * Yields binary chunks as Uint8Arrays. After a complete run, the bytes are
   * cached and future calls will yield the cached Uint8Array instead.
   */
  async *arrayBufferStream() {
    if (this._cache) {
      yield this._cache;
      return;
    }

    const response = await this.fetch(this._input);
    let result;
    // Use streams, if supported, so that we can show in-progress data instead
    // of waiting for the entire data file to download. The file can be >100 MB,
    // so streams ensure slow connections still see some data.
    if (response.body) {
      const reader = response.body.getReader();

      /** @type {Uint8Array[]} Store received bytes to merge later */
      let buffer = [];
      /** Total size of received bytes */
      let byteSize = 0;
      while (true) {
        // Read values from the stream
        const {done, value} = await reader.read();
        if (done) break;

        const chunk = new Uint8Array(value, 0, value.length);
        yield chunk;
        buffer.push(chunk);
        byteSize += chunk.length;
      }

      // We just cache a single typed array to save some memory and make future
      // runs consistent with the no streams mode.
      result = new Uint8Array(byteSize);
      let i = 0;
      for (const chunk of buffer) {
        result.set(chunk, i);
        i += chunk.length;
      }
    } else {
      // In-memory version for browsers without stream support
      yield result;
    }

    this._cache = result;
  }

  /**
   * Outputs a single UInt8Array containing the entire input .size file.
   */
  async loadSizeBuffer() {
    // Flush cache.
    for await (const bytes of this.arrayBufferStream()) {
      if (this._controller.signal.aborted) {
        throw new DOMException('Request was aborted', 'AbortError');
      }
    }
    return this._cache;
  }
}

function mallocBuffer(buf) {
  var dataPtr = Module._malloc(buf.byteLength);
  var dataHeap = new Uint8Array(Module.HEAPU8.buffer, dataPtr, buf.byteLength);
  dataHeap.set(new Uint8Array(buf));
  return dataHeap;
}

var _Open = null;
function Open(name) {
  if (_Open === null) {
    _Open = Module.cwrap('Open', 'number', ['string']);
  }
  let stringPtr = _Open(name);
  // Something has gone wrong if we get back a string longer than 67MB.
  let ret = JSON.parse(Module.UTF8ToString(stringPtr, 2 ** 26));
  console.log(ret);
  return ret;
}

// Placeholder input name until supplied via setInput()
const fetcher = new DataFetcher('data.size');

async function buildTree(
    groupBy, filterTest, highlightTest, methodCountMode, onProgress) {

  let sizeBuffer = await fetcher.loadSizeBuffer();
  let heapBuffer = mallocBuffer(sizeBuffer);
  console.log('Passing ' + sizeBuffer.byteLength + ' bytes to WebAssembly');
  let LoadSizeFile = Module.cwrap('LoadSizeFile', 'bool', ['number', 'number']);
  let start_time = Date.now();
  console.log(LoadSizeFile(heapBuffer.byteOffset, sizeBuffer.byteLength));
  console.log('Loaded size file in ' + (Date.now() - start_time)/1000.0 + ' seconds');
  Module._free(heapBuffer.byteOffset);

  /**
   * Creates data to post to the UI thread. Defaults will be used for the root
   * and percent values if not specified.
   * @param {{root?:TreeNode,percent?:number,error?:Error}} data Default data
   * values to post.
   */
  function createProgressMessage(data = {}) {
    let {percent} = data;
    if (percent == null) {
      if (meta == null) {
        percent = 0;
      } else {
        percent = Math.max(builder.rootNode.size / meta.total, 0.1);
      }
    }

    const message = {
      root: builder.formatNode(data.root || builder.rootNode),
      percent,
      diffMode: meta && meta.diff_mode,
    };
    if (data.error) {
      message.error = data.error.message;
    }
    return message;
  }

  return {
    root: Open(''),
    percent: 1.0,
    diffMode: 0, // diff mode
  };
}

/**
 * Parse the options represented as a query string, into an object.
 * Includes checks for valid values.
 * @param {string} options Query string
 */
function parseOptions(options) {
  const params = new URLSearchParams(options);

  const url = params.get('load_url');
  const groupBy = params.get('group_by') || 'source_path';
  const methodCountMode = params.has('method_count');
  const filterGeneratedFiles = params.has('generated_filter');
  const flagToHighlight = _NAMES_TO_FLAGS[params.get('highlight')];

  function filterTest(symbolNode) {
    return true;
  }
  function highlightTest(symbolNode) {
    return false;
  }
  return {groupBy, filterTest, highlightTest, url, methodCountMode};
}

const actions = {
  /** @param {{input:string|null,options:string}} param0 */
  load({input, options}) {
    const {groupBy, filterTest, highlightTest, url, methodCountMode} =
        parseOptions(options);
    if (input === 'from-url://' && url) {
      // Display the data from the `load_url` query parameter
      console.info('Displaying data from', url);
      fetcher.setInput(url);
    } else if (input != null) {
      console.info('Displaying uploaded data');
      fetcher.setInput(input);
    }

    return buildTree(
        groupBy, filterTest, highlightTest, methodCountMode, progress => {
          // @ts-ignore
          self.postMessage(progress);
        });
  },
  /** @param {string} path */
  async open(path) {
    return Open(path);
  },
};

/**
 * Call the requested action function with the given data. If an error is thrown
 * or rejected, post the error message to the UI thread.
 * @param {number} id Unique message ID.
 * @param {string} action Action type, corresponding to a key in `actions.`
 * @param {any} data Data to supply to the action function.
 */
async function runAction(id, action, data) {
  try {
    const result = await actions[action](data);
    // @ts-ignore
    self.postMessage({id, result});
  } catch (err) {
    // @ts-ignore
    self.postMessage({id, error: err.message});
    throw err;
  }
}

const runActionDebounced = debounce(runAction, 0);

/**
 * @param {MessageEvent} event Event for when this worker receives a message.
 */
self.onmessage = async event => {
  const {id, action, data} = event.data;
  if (action === 'load') {
    // Loading large files will block the worker thread until complete or when
    // an await statement is reached. During this time, multiple load messages
    // can pile up due to filters being adjusted. We debounce the load call
    // so that only the last message is read (the current set of filters).
    runActionDebounced(id, action, data);
  } else {
    runAction(id, action, data);
  }
};

