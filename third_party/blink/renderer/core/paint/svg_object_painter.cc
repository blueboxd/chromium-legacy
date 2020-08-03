// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/svg_object_painter.h"

#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

void SVGObjectPainter::PaintResourceSubtree(GraphicsContext& context) {
  DCHECK(!layout_object_.NeedsLayout());

  PaintInfo info(context, LayoutRect::InfiniteIntRect(),
                 PaintPhase::kForeground,
                 kGlobalPaintNormalPhase | kGlobalPaintFlattenCompositingLayers,
                 kPaintLayerPaintingRenderingResourceSubtree,
                 &layout_object_.PaintingLayer()->GetLayoutObject());
  layout_object_.Paint(info);
}

bool SVGObjectPainter::PreparePaint(
    const PaintInfo& paint_info,
    const ComputedStyle& style,
    LayoutSVGResourceMode resource_mode,
    PaintFlags& flags,
    const AffineTransform* additional_paint_server_transform) {
  if (paint_info.IsRenderingClipPathAsMaskImage()) {
    if (resource_mode == kApplyToStrokeMode)
      return false;
    flags.setColor(SK_ColorBLACK);
    flags.setShader(nullptr);
    return true;
  }

  SVGPaintServer paint_server = SVGPaintServer::RequestForLayoutObject(
      layout_object_, style, resource_mode);
  if (!paint_server.IsValid())
    return false;

  if (additional_paint_server_transform && paint_server.IsTransformDependent())
    paint_server.PrependTransform(*additional_paint_server_transform);

  const SVGComputedStyle& svg_style = style.SvgStyle();
  float alpha = resource_mode == kApplyToFillMode ? svg_style.FillOpacity()
                                                  : svg_style.StrokeOpacity();
  paint_server.ApplyToPaintFlags(flags, alpha);

  // We always set filter quality to 'low' here. This value will only have an
  // effect for patterns, which are SkPictures, so using high-order filter
  // should have little effect on the overall quality.
  flags.setFilterQuality(kLow_SkFilterQuality);

  // TODO(fs): The color filter can set when generating a picture for a mask -
  // due to color-interpolation. We could also just apply the
  // color-interpolation property from the the shape itself (which could mean
  // the paintserver if it has it specified), since that would be more in line
  // with the spec for color-interpolation. For now, just steal it from the GC
  // though.
  // Additionally, it's not really safe/guaranteed to be correct, as
  // something down the flags pipe may want to farther tweak the color
  // filter, which could yield incorrect results. (Consider just using
  // saveLayer() w/ this color filter explicitly instead.)
  flags.setColorFilter(sk_ref_sp(paint_info.context.GetColorFilter()));
  return true;
}

}  // namespace blink
