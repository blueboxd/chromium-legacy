// META: script=/resources/testharness.js
// META: script=/resources/testharnessreport.js
// META: script=/gen/layout_test_data/mojo/public/js/mojo_bindings.js
// META: script=/gen/mojo/public/mojom/base/unguessable_token.mojom.js
// META: script=/gen/third_party/blink/public/mojom/serial/serial.mojom.js
// META: script=resources/serial-test-utils.js

serial_test(async (t, fake) => {
  const {port, fakePort} = await getFakeSerialPort(fake);
  await promise_rejects_dom(t, 'InvalidStateError', port.setSignals({}));
}, 'setSignals() rejects if the port is not open');

serial_test(async (t, fake) => {
  const {port, fakePort} = await getFakeSerialPort(fake);
  await port.open({baudrate: 9600});

  let expectedSignals = {dtr: true, rts: false, brk: false};
  assert_object_equals(fakePort.outputSignals, expectedSignals, 'initial');

  await promise_rejects_js(t, TypeError, port.setSignals());
  assert_object_equals(fakePort.outputSignals, expectedSignals, 'no-op');

  await promise_rejects_js(t, TypeError, port.setSignals({}));
  assert_object_equals(fakePort.outputSignals, expectedSignals, 'no-op');

  await port.setSignals({dtr: false});
  expectedSignals.dtr = false;
  assert_object_equals(fakePort.outputSignals, expectedSignals, 'clear DTR');

  await port.setSignals({rts: true});
  expectedSignals.rts = true;
  assert_object_equals(fakePort.outputSignals, expectedSignals, 'set RTS');

  await port.setSignals({brk: true});
  expectedSignals.brk = true;
  assert_object_equals(fakePort.outputSignals, expectedSignals, 'set BRK');

  await port.setSignals({dtr: true, rts: false, brk: false});
  expectedSignals.dtr = true;
  expectedSignals.rts = false;
  expectedSignals.brk = false;
  assert_object_equals(fakePort.outputSignals, expectedSignals, 'invert');
}, 'setSignals() modifies the state of the port');
