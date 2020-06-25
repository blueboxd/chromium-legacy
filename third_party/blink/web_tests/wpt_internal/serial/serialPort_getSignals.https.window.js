// META: script=/resources/testharness.js
// META: script=/resources/testharnessreport.js
// META: script=/gen/layout_test_data/mojo/public/js/mojo_bindings.js
// META: script=/gen/mojo/public/mojom/base/unguessable_token.mojom.js
// META: script=/gen/third_party/blink/public/mojom/serial/serial.mojom.js
// META: script=resources/serial-test-utils.js

serial_test(async (t, fake) => {
  const {port, fakePort} = await getFakeSerialPort(fake);
  await promise_rejects_dom(t, 'InvalidStateError', port.getSignals());
}, 'getSignals() rejects if the port is not open');

serial_test(async (t, fake) => {
  const {port, fakePort} = await getFakeSerialPort(fake);
  await port.open({baudrate: 9600});

  let expectedSignals = {dcd: false, cts: false, ri: false, dsr: false};
  fakePort.simulateInputSignals(expectedSignals);
  let signals = await port.getSignals();
  assert_object_equals(signals, expectedSignals);

  expectedSignals.dcd = true;
  fakePort.simulateInputSignals(expectedSignals);
  signals = await port.getSignals();
  assert_object_equals(signals, expectedSignals, 'DCD set');

  expectedSignals.cts = true;
  fakePort.simulateInputSignals(expectedSignals);
  signals = await port.getSignals();
  assert_object_equals(signals, expectedSignals, 'CTS set');

  expectedSignals.ri = true;
  fakePort.simulateInputSignals(expectedSignals);
  signals = await port.getSignals();
  assert_object_equals(signals, expectedSignals, 'RI set');

  expectedSignals.dsr = true;
  fakePort.simulateInputSignals(expectedSignals);
  signals = await port.getSignals();
  assert_object_equals(signals, expectedSignals, 'DSR set');
}, 'getSignals() returns the current state of input control signals');
