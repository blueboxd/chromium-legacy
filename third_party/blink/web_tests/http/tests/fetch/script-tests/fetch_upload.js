if (self.importScripts) {
  importScripts('../resources/fetch-test-helpers.js');
}

function fetch_echo(stream) {
  return fetch(
      '/serviceworker/resources/fetch-echo-body.php',
      {method: 'POST', body: stream});
}

function fetch_echo_body(stream) {
  return fetch_echo(stream).then(response => response.text());
}

function create_stream(contents) {
  return new ReadableStream({
    start: controller => {
      for (obj of contents) {
        controller.enqueue(obj);
      }
      controller.close();
    }
  });
}

promise_test(() => {
  const stream =
      create_stream(['Foo', 'Bar']).pipeThrough(new TextEncoderStream());
  return fetch_echo_body(stream).then(function(text) {
    assert_equals(text, 'FooBar');
  });
}, 'Fetch with ReadableStream body with Uint8Array');

promise_test(() => {
  const stream = create_stream([new Uint8Array(0)]);
  return fetch_echo_body(stream).then(function(text) {
    assert_equals(text, '');
  });
}, 'Fetch with ReadableStream body with empty Uint8Array');

function random_values_array(length) {
  const array = new Uint8Array(length);
  // crypto.getRandomValues has a quota. See
  // https://www.w3.org/TR/WebCryptoAPI/#Crypto-method-getRandomValues.
  const cryptoQuota = 65535;
  let index = 0;
  const buffer = array.buffer;
  while (index < buffer.byteLength) {
    const bufferView = array.subarray(index, index + cryptoQuota);
    crypto.getRandomValues(bufferView);
    index += cryptoQuota;
  }
  return array;
}

function hash256(array) {
  return crypto.subtle.digest('SHA-256', array);
}

promise_test(async () => {
  const length = 1000 * 1000;  // 1Mbytes
  const array = random_values_array(length);
  const stream = create_stream([array]);
  const response = await fetch_echo(stream);
  const reader = response.body.getReader();
  let index = 0;
  while (index < length) {
    const chunk = await reader.read();
    assert_false(chunk.done);
    const chunk_length = chunk.value.length;
    const chunk_hash = await hash256(chunk.value);
    const src_hash = await hash256(array.subarray(index, index + chunk_length));
    assert_array_equals(
        new Uint8Array(chunk_hash), new Uint8Array(src_hash),
        `Array of [${index}, ${index + length - 1}] should be same.`);
    index += chunk_length;
  }
  const final_chunk = await reader.read();
  assert_true(final_chunk.done);
}, 'Fetch with ReadableStream body with long Uint8Array');

promise_test(async (t) => {
  const stream = create_stream(['Foobar']);
  await promise_rejects_js(t, TypeError, fetch_echo(stream));
}, 'Fetch with ReadableStream body containing String should fail');

promise_test(async (t) => {
  const stream = create_stream([null]);
  await promise_rejects_js(t, TypeError, fetch_echo(stream));
}, 'Fetch with ReadableStream body containing null should fail');

promise_test(async (t) => {
  const stream = create_stream([42]);
  await promise_rejects_js(t, TypeError, fetch_echo(stream));
}, 'Fetch with ReadableStream body containing number should fail');

done();
