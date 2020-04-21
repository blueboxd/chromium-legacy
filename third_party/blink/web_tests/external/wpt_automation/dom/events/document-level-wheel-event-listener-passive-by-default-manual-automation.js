function wheelScroll(direction, start_x, start_y) {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      chrome.gpuBenchmarking.smoothScrollBy(100 /* pixels to scroll */,
                                            resolve,
                                            start_x,
                                            start_y,
                                            2 /* wheel scroll source */,
                                            direction);
    } else {
      reject();
    }
  });
}

{
  var input_injection = async_test("Input Injection Automation");
  // Returns a promise that gets resolved when input injection is finished.
  window.requestAnimationFrame(
    function () {
      inject_input().then(function() {
        input_injection.done();
      });
    });
}

function inject_input() {
  return smoothScrollBy(100, window.innerWidth / 2, window.innerHeight / 2, 'down', chrome.gpuBenchmarking.MOUSE_INPUT);
}
