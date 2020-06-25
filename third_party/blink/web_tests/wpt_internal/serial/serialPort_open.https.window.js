// META: script=/resources/testharness.js
// META: script=/resources/testharnessreport.js
// META: script=/gen/layout_test_data/mojo/public/js/mojo_bindings.js
// META: script=/gen/mojo/public/mojom/base/unguessable_token.mojom.js
// META: script=/gen/third_party/blink/public/mojom/serial/serial.mojom.js
// META: script=resources/serial-test-utils.js

serial_test(async (t, fake) => {
  const {port, fakePort} = await getFakeSerialPort(fake);

  await port.open({baudrate: 9600});
  return promise_rejects_dom(
      t, 'InvalidStateError', port.open({baudrate: 9600}));
}, 'A SerialPort cannot be opened if it is already open.');

serial_test(async (t, fake) => {
  const {port, fakePort} = await getFakeSerialPort(fake);

  const firstRequest = port.open({baudrate: 9600});
  await promise_rejects_dom(
      t, 'InvalidStateError', port.open({baudrate: 9600}));
  await firstRequest;
}, 'Simultaneous calls to open() are disallowed.');

serial_test(async (t, fake) => {
  const {port, fakePort} = await getFakeSerialPort(fake);

  await promise_rejects_js(t, TypeError, port.open({}));

  await Promise.all([-1, 0].map(
      baudrate => {
          return promise_rejects_js(t, TypeError, port.open({baudrate}))}));
}, 'Baud rate is required and must be greater than zero.');

serial_test(async (t, fake) => {
  const {port, fakePort} = await getFakeSerialPort(fake);

  await Promise.all([-1, 0, 6, 9].map(databits => {
    return promise_rejects_js(
        t, TypeError, port.open({baudrate: 9600, databits}));
  }));

  await[undefined, 7, 8].reduce(async (previousTest, databits) => {
    await previousTest;
    await port.open({baudrate: 9600, databits});
    await port.close();
  }, Promise.resolve());
}, 'Data bits must be 7 or 8');

serial_test(async (t, fake) => {
  const {port, fakePort} = await getFakeSerialPort(fake);

  await Promise.all([0, null, 'cats'].map(parity => {
    return promise_rejects_js(
        t, TypeError, port.open({baudrate: 9600, parity}),
        `Should reject parity option "${parity}"`);
  }));

  await[undefined, 'none', 'even', 'odd'].reduce(
      async (previousTest, parity) => {
        await previousTest;
        await port.open({baudrate: 9600, parity});
        await port.close();
      },
      Promise.resolve());
}, 'Parity must be "none", "even" or "odd"');

serial_test(async (t, fake) => {
  const {port, fakePort} = await getFakeSerialPort(fake);

  await Promise.all([-1, 0, 3, 4].map(stopbits => {
    return promise_rejects_js(
        t, TypeError, port.open({baudrate: 9600, stopbits}));
  }));

  await[undefined, 1, 2].reduce(async (previousTest, stopbits) => {
    await previousTest;
    await port.open({baudrate: 9600, stopbits});
    await port.close();
  }, Promise.resolve());
}, 'Stop bits must be 1 or 2');

serial_test(async (t, fake) => {
  const {port, fakePort} = await getFakeSerialPort(fake);

  await promise_rejects_js(
      t, TypeError, port.open({baudrate: 9600, buffersize: -1}));
  await promise_rejects_js(
      t, TypeError, port.open({baudrate: 9600, buffersize: 0}));
}, 'Buffer size must be greater than zero.');

serial_test(async (t, fake) => {
  const {port, fakePort} = await getFakeSerialPort(fake);

  const buffersize = 1 * 1024 * 1024 * 1024 /* 1 GiB */;
  return promise_rejects_js(
      t, TypeError, port.open({baudrate: 9600, buffersize}));
}, 'Unreasonably large buffer sizes are rejected.');
