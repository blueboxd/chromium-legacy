(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div id='node' style='background-color: red; height: 100px'></div>
  `, 'Tests animation started and records composite failure reasons in trace event.');
  var TracingHelper = await testRunner.loadScript('../resources/tracing-test.js');
  var tracingHelper = new TracingHelper(testRunner, session);

  dp.Animation.enable();
  await tracingHelper.startTracing();

  session.evaluate(`var animation = node.animate([{ width: '100px' }, { width: '200px' }], 500);`);
  await dp.Animation.onceAnimationCreated();
  testRunner.log('Animation created');
  await dp.Animation.onceAnimationStarted();
  testRunner.log('Animation started');
  session.evaluate(`animation.cancel()`);
  await dp.Animation.onceAnimationCanceled();

  await tracingHelper.stopTracing();

  var animationEvents = tracingHelper.filterEvents(e => e.name === 'Animation');
  animationEvents.forEach(e => testRunner.log(`Name:${e.name},Phase:${e.ph}`));
  var animation = animationEvents[1];
  testRunner.log('Animation composite failed reasons: ' + animation.args.data.compositeFailed);

  testRunner.completeTest();
})
