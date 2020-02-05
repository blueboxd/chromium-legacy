// |kOverlayInterstitialAd| from web_feature.mojom.
const kOverlayInterstitialAd = 3156;

function verifyInterstitialUseCounterAfter1500ms(
    test, expect_use_counter, post_verify_callback) {
  // Force a lifecycle update after 1500ms, so that the final stable layout
  // can be captured.
  setTimeout(
      test.step_func(() => {
        requestAnimationFrame(test.step_func(() => {
          // This runs just prior to the lifecycle update.
          setTimeout(test.step_func(() => {
            // This code runs immediately after the lifecycle update.
            if (self.internals)
              assert_equals(
                  internals.isUseCounted(document, kOverlayInterstitialAd),
                  expect_use_counter);

            if (post_verify_callback !== undefined) {
              post_verify_callback(test);
            } else {
              test.done();
            }
          }));
        }));
      }),
      1500);
}

// Set up div(div(iframe)) where the outer div has fixed position.
function createInterstitialAdFrameFixedOuterDiv() {
  let div1 = document.createElement('div');
  div1.style.position = 'fixed';
  div1.style.top = '0px';
  div1.style.left = '0px';
  div1.style.width = '100%';
  div1.style.height = '100%';

  let div2 = document.createElement('div');
  div2.style.width = '100%';
  div2.style.height = '100%';

  let ad_frame = document.createElement('iframe');
  ad_frame.style.width = '50%';
  ad_frame.style.height = '100%';

  div2.appendChild(ad_frame);
  div1.appendChild(div2);
  document.body.appendChild(div1);
  return ad_frame;
}

// Set up div(div(iframe)) where the frame has fixed position.
function createInterstitialAdFrameFixedFrame() {
  let div1 = document.createElement('div');
  div1.style.width = '100%';
  div1.style.height = '100%';

  let div2 = document.createElement('div');
  div2.style.width = '100%';
  div2.style.height = '100%';

  let ad_frame = document.createElement('iframe');
  ad_frame.style.position = 'fixed';
  ad_frame.style.top = '0px';
  ad_frame.style.left = '0px';
  ad_frame.style.width = '50%';
  ad_frame.style.height = '100%';

  div2.appendChild(ad_frame);
  div1.appendChild(div2);
  document.body.appendChild(div1);
  return ad_frame;
}

// Set up div(div(iframe)) where the outer div has absolute position.
function createInterstitialAdFrameAbsoluteOuterDiv() {
  let div1 = document.createElement('div');
  div1.style.position = 'absolute';
  div1.style.top = '0px';
  div1.style.left = '0px';
  div1.style.width = '100%';
  div1.style.height = '100%';

  let div2 = document.createElement('div');
  div2.style.width = '100%';
  div2.style.height = '100%';

  let ad_frame = document.createElement('iframe');
  ad_frame.style.width = '50%';
  ad_frame.style.height = '100%';

  div2.appendChild(ad_frame);
  div1.appendChild(div2);
  document.body.appendChild(div1);
  return ad_frame;
}
