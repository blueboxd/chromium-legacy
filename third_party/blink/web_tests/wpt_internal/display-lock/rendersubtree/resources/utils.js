
function setInvisible(element) {
  return setRenderSubtree(element, "invisible");
}

function setInvisibleActivatable(element) {
  return setRenderSubtree(element, "invisible activatable");
}

function setVisible(element) {
  return setRenderSubtree(element, "");
}

function setRenderSubtree(element, value) {
  element.setAttribute("rendersubtree", value);
  return new Promise((resolve, reject) => {
    // Returns a promise that resolves when the rendering changes take effect.
    // TODO(rakina): Change to requestPostAnimationFrame when available?
    requestAnimationFrame(() => {
      requestAnimationFrame(resolve);
    });
  });
}
