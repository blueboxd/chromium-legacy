# CSS Paint API

This directory contains the implementation of the CSS Paint API.

See [CSS Paint API](https://drafts.css-houdini.org/css-paint-api/) for the web exposed APIs this
implements.

## Implementation

### [CSSPaintDefinition](css_paint_definition.h)

Represents a class registered by the author through `PaintWorkletGlobalScope#registerPaint`.
Specifically this class holds onto the javascript constructor and paint functions of the class via
persistent handles. This class keeps these functions alive so they don't get garbage collected.

The `CSSPaintDefinition` also holds onto an instance of the paint class via a persistent handle. This
instance is lazily created upon first use. If the constructor throws for some reason the constructor
is marked as invalid and will always produce invalid images.

The `PaintWorkletGlobalScope` has a map of paint `name` to `CSSPaintDefinition`.

### [CSSPaintImageGenerator][generator] and [CSSPaintImageGeneratorImpl][generator-impl]

`CSSPaintImageGenerator` represents the interface from which the `CSSPaintValue` can generate
`Image`s. This is done via the `CSSPaintImageGenerator#paint` method. Each `CSSPaintValue` owns a
separate instance of `CSSPaintImageGenerator`.

`CSSPaintImageGeneratorImpl` is the implementation which lives in `modules/csspaint`. (We have this
interface / implementation split as `core/` cannot depend on `modules/`).

When created the generator will access its paint worklet and lookup it's corresponding
`CSSPaintDefinition` via `PaintWorkletGlobalScope#findDefinition`.

If the paint worklet does not have a `CSSPaintDefinition` matching the paint `name` the
`CSSPaintImageGeneratorImpl` is placed in a "pending" map. Once a paint class with `name` is
registered the generator is notified so it can invalidate an display the correct image.

[generator]: ../../core/css/css_paint_image_generator.h
[generator-impl]: css_paint_image_generator_impl.h
[paint-value]: ../../core/css/css_paint_value.h

### Generating a
[PaintGeneratedImage](../../platform/graphics/paint_generated_image.h)

`PaintGeneratedImage` is a `Image` which just paints a single `PaintRecord`.

A `CSSPaintValue` can generate an image from the method `CSSPaintImageGenerator#paint`. This method
calls through to `CSSPaintDefinition#paint` which actually invokes the javascript paint method.
This method returns the `PaintGeneratedImage`.

### Style Invalidation

The `CSSPaintDefinition` keeps a list of both native and custom properties it will invalidate on.
During style invalidation `ComputedStyle` checks if it has any `CSSPaintValue`s, and if any of their
properties have changed; if so it will invalidate paint for that `ComputedStyle`.

If the `CSSPaintValue` doesn't have a corresponding `CSSPaintDefinition` yet, it doesn't invalidate
paint.

## Testing

Tests live [here](../../../web_tests/http/tests/csspaint/) and
[here](../../../web_tests/external/wpt/css/css-paint-api/).

