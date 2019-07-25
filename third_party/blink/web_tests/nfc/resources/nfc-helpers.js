'use strict';

var test_text_data = 'Test text data.';
var test_text_byte_array = new TextEncoder('utf-8').encode(test_text_data);
var test_number_data = 42;
var test_json_data = {level: 1, score: 100, label: 'Game'};
var test_url_data = 'https://w3c.github.io/web-nfc/';
var test_message_origin = 'https://127.0.0.1:8443';
var test_buffer_data = new ArrayBuffer(test_text_byte_array.length);
var test_buffer_view = new Uint8Array(test_buffer_data).set(test_text_byte_array);
var fake_tag_serial_number = 'c0:45:00:02';

var NFCHWStatus = {};
// OS-level NFC setting is ON
NFCHWStatus.ENABLED = 1;
// no NFC chip
NFCHWStatus.NOT_SUPPORTED = NFCHWStatus.ENABLED + 1;
// OS-level NFC setting OFF
NFCHWStatus.DISABLED = NFCHWStatus.NOT_SUPPORTED + 1;

function noop() {}

function assertRejectsWithError(promise, name) {
  return promise.then(() => {
    assert_unreached('expected promise to reject with ' + name);
  }, error => {
    assert_equals(error.name, name);
  });
}

function createMessage(records) {
  if (records !== undefined) {
    let message = {}
    message.records = records;
    return message;
  }
}

function createRecord(recordType, mediaType, data) {
  let record = {};
  if (recordType !== undefined)
    record.recordType = recordType;
  if (mediaType !== undefined)
    record.mediaType = mediaType;
  if (data !== undefined)
    record.data = data;
  return record;
}

function createNFCPushOptions(target, timeout, ignoreRead, compatibility) {
  return { target, timeout, ignoreRead, compatibility};
}

function createNFCReaderOptions(url, recordType, mediaType, compatibility) {
  return { url, recordType, mediaType, compatibility };
}

function createTextRecord(text) {
  return createRecord('text', 'text/plain', text);
}

function createJsonRecord(json) {
  return createRecord('json', 'application/json', json);
}

function createOpaqueRecord(buffer) {
  return createRecord('opaque', 'application/octet-stream', buffer);
}

function createUrlRecord(url) {
  return createRecord('url', 'text/plain', url);
}

function toMojoNDEFRecordType(type) {
  switch (type) {
  case 'text':
    return device.mojom.NDEFRecordType.TEXT;
  case 'url':
    return device.mojom.NDEFRecordType.URL;
  case 'json':
    return device.mojom.NDEFRecordType.JSON;
  case 'opaque':
    return device.mojom.NDEFRecordType.OPAQUE_RECORD;
  }

  return device.mojom.NDEFRecordType.EMPTY;
}

function toMojoNFCPushTarget(target) {
  switch (target) {
  case 'any':
    return device.mojom.NFCPushTarget.ANY;
  case 'peer':
    return device.mojom.NFCPushTarget.PEER;
  case 'tag':
    return device.mojom.NFCPushTarget.TAG;
  }

  return device.mojom.NFCPushTarget.ANY;
}

function toMojoNDEFCompatibility(compatibility) {
  if (compatibility === 'nfc-forum')
    return device.mojom.NDEFCompatibility.NFC_FORUM;
  if (compatibility === 'vendor')
    return device.mojom.NDEFCompatibility.VENDOR;
  return device.mojom.NDEFCompatibility.ANY;
}

// Converts between NDEFMessageInit https://w3c.github.io/web-nfc/#dom-ndefmessage
// and mojom.NDEFMessage structure, so that nfc.watch function can be tested.
function toMojoNDEFMessage(message) {
  let ndefMessage = new device.mojom.NDEFMessage();
  ndefMessage.url = message.url;
  ndefMessage.data = [];
  for (let record of message.records)
    ndefMessage.data.push(toMojoNDEFRecord(record));
  return ndefMessage;
}

function toMojoNDEFRecord(record) {
  let nfcRecord = new device.mojom.NDEFRecord();
  nfcRecord.recordType = toMojoNDEFRecordType(record.recordType);
  nfcRecord.mediaType = record.mediaType;
  nfcRecord.data = toByteArray(record.data);
  return nfcRecord;
}

function toByteArray(data) {
  // Convert JS objects to byte array
  let byteArray;
  let tmpData = data;

  if (tmpData instanceof ArrayBuffer)
    byteArray = new Uint8Array(tmpData);
  else if (typeof tmpData === 'object' || typeof tmpData === 'number')
    tmpData = JSON.stringify(tmpData);

  if (typeof tmpData === 'string')
    byteArray = new TextEncoder('utf-8').encode(tmpData);

  return byteArray;
}

// Compares NDEFMessageSource that was provided to the API
// (e.g. navigator.nfc.push), and mojom NDEFMessage that was received by the
// mock NFC service.
function assertNDEFMessagesEqual(providedMessage, receivedMessage) {
  // If simple data type is passed, e.g. String or ArrayBuffer, convert it
  // to NDEFMessage before comparing.
  // https://w3c.github.io/web-nfc/#dom-ndefmessagesource
  let provided = providedMessage;
  if (providedMessage instanceof ArrayBuffer)
    provided = createMessage([createOpaqueRecord(providedMessage)]);
  else if (typeof providedMessage === 'string')
    provided = createMessage([createTextRecord(providedMessage)]);

  assert_equals(provided.records.length, receivedMessage.data.length,
      'NDEFMessages must have same number of NDEFRecords');

  // Compare contents of each individual NDEFRecord
  for (let i = 0; i < provided.records.length; ++i)
    compareNDEFRecords(provided.records[i], receivedMessage.data[i]);
}

// Used to compare two WebNFC messages, one that is provided to mock NFC
// service through triggerWatchCallback and another that is received by
// callback that is provided to navigator.nfc.watch function.
function assertWebNDEFMessagesEqual(a, b) {
  assert_equals(a.url, b.url);
  assert_equals(a.records.length, b.records.length);
  for(let i in a.records) {
    let recordA = a.records[i];
    let recordB = b.records[i];
    assert_equals(recordA.recordType, recordB.recordType);
    assert_equals(recordA.mediaType, recordB.mediaType);
    if (recordA.data instanceof ArrayBuffer) {
      assert_array_equals(new Uint8Array(recordA.data),
          new Uint8Array(recordB.data));
    } else if (typeof recordA.data === 'object') {
      assert_object_equals(recordA.data, recordB.data);
    }

    if (typeof recordA.data === 'number'
        || typeof recordA.data === 'string') {
      assert_true(recordA.data == recordB.data);
    }
  }
}

// Compares NDEFRecords that were provided / received by the mock service.
function compareNDEFRecords(providedRecord, receivedRecord) {
  assert_equals(toMojoNDEFRecordType(providedRecord.recordType),
                receivedRecord.recordType);

  // Compare media types without charset.
  // Charset should be compared when watch method is implemented, in order
  // to check that written and read strings are equal.
  assert_equals(providedRecord.mediaType,
      receivedRecord.mediaType.substring(0, providedRecord.mediaType.length));

  assert_false(toMojoNDEFRecordType(providedRecord.recordType) ==
              device.mojom.NDEFRecordType.EMPTY);

  assert_array_equals(toByteArray(providedRecord.data),
                      new Uint8Array(receivedRecord.data));
}

// Compares NFCPushOptions structures that were provided to API and
// received by the mock mojo service.
function assertNFCPushOptionsEqual(provided, received) {
  if (provided.ignoreRead !== undefined)
    assert_equals(provided.ignoreRead, !!+received.ignoreRead);
  else
    assert_equals(!!+received.ignore_read, true);

  if (provided.timeout !== undefined)
    assert_equals(provided.timeout, received.timeout);
  else
    assert_equals(received.timeout, Infinity);

  if (provided.target !== undefined)
    assert_equals(toMojoNFCPushTarget(provided.target), received.target);
  else
    assert_equals(received.target, device.mojom.NFCPushTarget.ANY);

  if (provided.compatibility !== undefined)
    assert_equals(toMojoNDEFCompatibility(provided.compatibility), received.compatibility);
  else
    assert_equals(received.compatibility, device.mojom.NDEFCompatibility.NFC_FORUM);
}

// Compares NFCReaderOptions structures that were provided to API and
// received by the mock mojo service.
function assertNFCReaderOptionsEqual(provided, received) {
  if (provided.url !== undefined)
    assert_equals(provided.url, received.url);
  else
    assert_equals(received.url, '');

  if (provided.mediaType !== undefined)
    assert_equals(provided.mediaType, received.mediaType);
  else
    assert_equals(received.mediaType, '');

  if (provided.compatibility !== undefined)
    assert_equals(toMojoNDEFCompatibility(provided.compatibility), received.compatibility);
  else
    assert_equals(received.compatibility, device.mojom.NDEFCompatibility.NFC_FORUM);

  if (provided.recordType !== undefined) {
    assert_equals(!+received.record_filter, true);
    assert_equals(toMojoNDEFRecordType(provided.recordType),
        received.recordFilter.recordType);
  }
}

function createNFCError(type) {
  return { error: type ?
      new device.mojom.NFCError({ errorType: type }) : null };
}

// A helper for forwarding MojoHandle instances from one frame to another.
class CrossFrameHandleProxy {
  constructor(callback) {
    let {handle0, handle1} = Mojo.createMessagePipe();
    this.sender_ = handle0;
    this.receiver_ = handle1;
    this.receiver_.watch({readable: true}, () => {
      var message = this.receiver_.readMessage();
      callback(message.handles[0]);
    });
  }

  forwardHandle(handle) {
    this.sender_.writeMessage(new ArrayBuffer, [handle]);
  }
}

class MockNFC {
  constructor() {
    this.bindingSet_ = new mojo.BindingSet(device.mojom.NFC);

    this.interceptor_ = new MojoInterfaceInterceptor(
        device.mojom.NFC.name);
    this.interceptor_.oninterfacerequest =
        e => this.bindingSet_.addBinding(this, e.handle);
    this.interceptor_.start();
    this.crossFrameHandleProxy_ = new CrossFrameHandleProxy(
        handle => this.bindingSet_.addBinding(this, handle));

    this.hw_status_ = NFCHWStatus.ENABLED;
    this.pushed_message_ = null;
    this.push_options_ = null;
    this.pending_promise_func_ = null;
    this.push_timeout_id_ = null;
    this.push_completed_ = true;
    this.client_ = null;
    this.watch_id_ = 0;
    this.watchers_ = [];
  }

  attachToWindow(otherWindow) {
    otherWindow.nfcInterceptor =
        new otherWindow.MojoInterfaceInterceptor(
            device.mojom.NFC.name);
    otherWindow.nfcInterceptor.oninterfacerequest =
        e => this.crossFrameHandleProxy_.forwardHandle(e.handle);
    otherWindow.nfcInterceptor.start();
  }


  // NFC delegate functions
  async push(message, options) {
    let error = this.isReady();
    if (error)
      return error;

    this.pushed_message_ = message;
    this.push_options_ = options;

    return new Promise(resolve => {
      this.pending_promise_func_ = resolve;
      if (options.timeout && options.timeout !== Infinity &&
          !this.push_completed_) {
        this.push_timeout_id_ =
            window.setTimeout(() => {
                resolve(
                    createNFCError(device.mojom.NFCErrorType.TIMER_EXPIRED));
            }, options.timeout);
      } else {
        resolve(createNFCError(null));
      }
    });
  }

  async cancelPush(target) {
    if (this.push_options_ && ((target === device.mojom.NFCPushTarget.ANY) ||
        (this.push_options_.target === target))) {
      this.cancelPendingPushOperation();
    }

    return createNFCError(null);
  }

  setClient(client) {
    this.client_ = client;
  }

  async watch(options) {
    let error = this.isReady();
    if (error) {
      error.id = 0;
      return error;
    }

    let retVal = createNFCError(null);
    retVal.id = ++this.watch_id_;
    this.watchers_.push({id: this.watch_id_, options: options});
    return retVal;
  }

  async cancelWatch(id) {
    let index = this.watchers_.findIndex(value => value.id === id);
    if (index === -1) {
      return createNFCError(device.mojom.NFCErrorType.NOT_FOUND);
    }

    this.watchers_.splice(index, 1);
    return createNFCError(null);
  }

  async cancelAllWatches() {
    if (this.watchers_.length === 0) {
      return createNFCError(device.mojom.NFCErrorType.NOT_FOUND);
    }

    this.watchers_.splice(0, this.watchers_.length);
    return createNFCError(null);
  }

  isReady() {
    if (this.hw_status_ === NFCHWStatus.DISABLED)
      return createNFCError(device.mojom.NFCErrorType.NOT_READABLE);
    if (this.hw_status_ === NFCHWStatus.NOT_SUPPORTED)
      return createNFCError(device.mojom.NFCErrorType.NOT_READABLE);
    return null;
  }

  setHWStatus(status) {
    this.hw_status_ = status;
  }

  pushedMessage() {
    return this.pushed_message_;
  }

  pushOptions() {
    return this.push_options_;
  }

  watchOptions() {
    assert_not_equals(this.watchers_.length, 0);
    return this.watchers_[this.watchers_.length - 1].options;
  }

  setPendingPushCompleted(result) {
    this.push_completed_ = result;
  }

  reset() {
    this.hw_status_ = NFCHWStatus.ENABLED;
    this.push_completed_ = true;
    this.watch_id_ = 0;
    this.watchers_ = [];
    this.cancelPendingPushOperation();
  }

  cancelPendingPushOperation() {
    if (this.push_timeout_id_) {
      window.clearTimeout(this.push_timeout_id_);
    }

    if (this.pending_promise_func_) {
      this.pending_promise_func_(
          createNFCError(device.mojom.NFCErrorType.OPERATION_CANCELLED));
    }

    this.pushed_message_ = null;
    this.push_options_ = null;
    this.pending_promise_func_ = null;
  }

  triggerWatchCallback(id, message) {
    assert_true(this.client_ !== null);
    if (this.watchers_.length > 0) {
      this.client_.onWatch([id], fake_tag_serial_number, toMojoNDEFMessage(message));
    }
  }
}

let mockNFC = new MockNFC();

function nfc_test(func, name, properties) {
  promise_test(async function() {
    try {
      await Promise.resolve(func());
    } finally {
      mockNFC.reset();
    }
  }, name, properties);
}
