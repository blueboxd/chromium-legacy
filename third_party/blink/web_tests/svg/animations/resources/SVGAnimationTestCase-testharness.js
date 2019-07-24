// Inspired by Layoutests/animations/animation-test-helpers.js
function moveAnimationTimelineAndSample(index) {
    var animationId = expectedResults[index][0];
    var time = expectedResults[index][1];
    var sampleCallback = expectedResults[index][2];
    var animation = rootSVGElement.ownerDocument.getElementById(animationId);

    // If we want to sample the animation end, add a small delta, to reliable point past the end of the animation.
    newTime = time;

    // The sample time is relative to the start time of the animation, take that into account.
    rootSVGElement.setCurrentTime(newTime);
    sampleCallback();
}

var currentTest = 0;
var expectedResults;

function sampleAnimation(t) {
    if (currentTest == expectedResults.length) {
        t.done();
        return;
    }

    moveAnimationTimelineAndSample(currentTest);
    ++currentTest;

    setTimeout(t.step_func(function () { sampleAnimation(t); }), 0);
}

function runAnimationTest(t, expected) {
    if (!expected)
        throw("Expected results are missing!");
    if (currentTest > 0)
        throw("Not allowed to call runAnimationTest() twice");

    expectedResults = expected;
    testCount = expectedResults.length;
    currentTest = 0;

    // Immediately sample, if the first time is zero.
    if (expectedResults[0][1] == 0) {
        expectedResults[0][2]();
        ++currentTest;
    }

  setTimeout(t.step_func(function () { sampleAnimation(this); }), 50);
}

function smil_async_test(func) {
  async_test(t => {
    window.onload = t.step_func(function () {
      // Pause animations, we'll drive them manually.
      // This also ensures that the timeline is paused before
      // it starts. This should make the instance time of the below
      // 'click' (for instance) 0, and hence minimize rounding
      // errors for the addition in moveAnimationTimelineAndSample.
      rootSVGElement.pauseAnimations();

      // If eg. an animation is running with begin="0s", and
      // we want to sample the first time, before the animation
      // starts, then we can't delay the testing by using an
      // onclick event, as the animation would be past start time.
      func(t);
    });
  });
}
