importAutomationScript('/pointerevents/pointerevent_common_input.js');

function waitForAnimationEnd() {
  const MAX_FRAME = 500;
  var last_changed_frame = 0;
  var last_window_x = window.scrollX;
  var last_window_y = window.scrollY;
  return new Promise((resolve, reject) => {
    function tick(frames) {
      // We requestAnimationFrame either for 500 frames or until 15 frames with
      // no change have been observed.
      if (frames >= MAX_FRAME || frames - last_changed_frame > 15) {
        resolve();
      } else {
        if (window.scrollX != last_window_x ||
            window.scrollY != last_window_y) {
          last_changed_frame = frames;
          last_window_x = window.scrollX;
          last_window_y = window.scrollY;
        }
        requestAnimationFrame(tick.bind(this, frames + 1));
      }
    }
    tick(0);
  });
}

function inject_input() {
  return smoothScrollBy(100, 20, 20, "downright", chrome.gpuBenchmarking.MOUSE_INPUT, 4000).then(() => {
    return waitForAnimationEnd();
  }).then(() => {
    return mouseClickInTarget('#btn', undefined, /* left button */ 0,
        /* shouldScrollToTarget = */ false);
  });
}
