// |kOverlayPopupAd| from web_feature.mojom.
const kOverlayPopupAd = 3253;

function verifyOverlayPopupUseCounterAfter1500ms(
    test, expect_use_counter, post_verify_callback) {
  // Force a lifecycle update after 1500ms, so that the final stable layout
  // can be captured.
  setTimeout(
      test.step_func(() => {
        requestAnimationFrame(test.step_func(() => {
          setTimeout(test.step_func(() => {
            // This code runs immediately after the lifecycle update.
            if (self.internals) {
              assert_equals(
                  internals.isUseCounted(document, kOverlayPopupAd),
                  expect_use_counter);
            }

            if (post_verify_callback !== undefined) {
              post_verify_callback();
            } else {
              test.done();
            }
          }));
        }));
      }),
      1500);
}

function appendAdFrameTo(parent)  {
  let ad_frame = document.createElement('iframe');
  parent.appendChild(ad_frame);
  return ad_frame;
}
