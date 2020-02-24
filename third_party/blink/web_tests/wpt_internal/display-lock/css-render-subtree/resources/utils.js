function addClassAndProcessLifecycle(element, value) {
  element.classList.add(value);
  return new Promise((resolve, reject) => {
    // Returns a promise that resolves when the rendering changes take effect.
    // TODO(rakina): Change to requestPostAnimationFrame when available?
    requestAnimationFrame(() => {
      requestAnimationFrame(resolve);
    });
  });
}

function removeClassAndProcessLifecycle(element, value) {
  element.classList.remove(value);
  return new Promise((resolve, reject) => {
    // Returns a promise that resolves when the rendering changes take effect.
    // TODO(rakina): Change to requestPostAnimationFrame when available?
    requestAnimationFrame(() => {
      requestAnimationFrame(resolve);
    });
  });
}
