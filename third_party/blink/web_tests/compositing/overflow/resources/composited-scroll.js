function layerSubtreeHasCompositedScrollLayer(layer, contents_opaque = undefined) {
    return layer.children && layer.children.some(function (child) {
        if (child.name == "Scrolling Contents Layer") {
            if (contents_opaque === undefined)
                return true;
            if (contents_opaque == !!child.contentsOpaque)
                return true;
        }
        if (layerSubtreeHasCompositedScrollLayer(child, contents_opaque))
            return true;
        return false;
    });
}

function elementSubtreeHasCompositedScrollLayers(element) {
    var layerTree = internals.elementLayerTreeAsText(element);
    if (layerTree === '')
        return false;
    var layer = JSON.parse(layerTree);
    return layerSubtreeHasCompositedScrollLayer(layer);
}

function elementSubtreeHasOpaqueCompositedScrollingContentsLayer(element) {
    var layerTree = internals.elementLayerTreeAsText(element);
    if (layerTree === '')
        return false;
    var layer = JSON.parse(layerTree);
    return layerSubtreeHasCompositedScrollLayer(layer, true);
}

function elementSubtreeHasNotOpaqueCompositedScrollingContentsLayer(element) {
    var layerTree = internals.elementLayerTreeAsText(element);
    if (layerTree === '')
        return false;
    var layer = JSON.parse(layerTree);
    return layerSubtreeHasCompositedScrollLayer(layer, false);
}