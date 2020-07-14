(async function(testRunner) {
  const {session, dp} = await testRunner.startHTML(
    `<link rel="stylesheet" href="${testRunner.url('./resources/noto-mono.css')}">
    some text`,
    'Verifies that CSS.fontsUpdated events are sent after CSS domain is enabled'
  );
  const eventPromise = dp.CSS.onceFontsUpdated(
    event => typeof event.params.font !== 'undefined' &&
    event.params.font.fontFamily === 'Noto Mono');
  await dp.DOM.enable();
  await dp.CSS.enable();
  const event = await eventPromise;
  const font = event.params.font;
  testRunner.log(font, null, ['src'])
  testRunner.log('SUCCESS: CSS.FontsUpdated events received.');
  testRunner.completeTest();
});
