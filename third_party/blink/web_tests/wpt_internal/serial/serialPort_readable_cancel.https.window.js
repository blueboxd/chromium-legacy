// META: script=/resources/testharness.js
// META: script=/resources/testharnessreport.js
// META: script=/gen/layout_test_data/mojo/public/js/mojo_bindings.js
// META: script=/gen/mojo/public/mojom/base/unguessable_token.mojom.js
// META: script=/gen/third_party/blink/public/mojom/serial/serial.mojom.js
// META: script=resources/serial-test-utils.js

serial_test(async (t, fake) => {
  const {port, fakePort} = await getFakeSerialPort(fake);
  // Select a buffer size smaller than the amount of data transferred.
  await port.open({baudrate: 9600, buffersize: 64});

  const reader = port.readable.getReader();
  const readPromise = reader.read();
  await reader.cancel();
  const {value, done} = await readPromise;
  assert_true(done);
  assert_equals(undefined, value);

  await port.close();
}, 'Can cancel while reading');
