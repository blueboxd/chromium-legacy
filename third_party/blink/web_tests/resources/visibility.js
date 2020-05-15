'use strict';

function setMainWindowHidden(hidden) {
  if (window.testRunner)
    testRunner.setMainWindowHidden(hidden);
  return new Promise((resolve, reject) => {
    if (document.visibilityState == (hidden ? "hidden" : "visible"))
      reject("setMainWindowHidden(" + hidden + ") called but already " + hidden);
    document.addEventListener("visibilitychange", resolve, {once:true});
  });
}
