/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/layout/svg/svg_marker_data.h"

namespace blink {

static double BisectingAngle(double in_angle, double out_angle) {
  // WK193015: Prevent bugs due to angles being non-continuous.
  if (fabs(in_angle - out_angle) > 180)
    in_angle += 360;
  return (in_angle + out_angle) / 2;
}

void SVGMarkerDataBuilder::Build(const Path& path) {
  path.Apply(this, SVGMarkerDataBuilder::UpdateFromPathElement);
  PathIsDone();
}

void SVGMarkerDataBuilder::UpdateFromPathElement(void* info,
                                                 const PathElement* element) {
  static_cast<SVGMarkerDataBuilder*>(info)->UpdateFromPathElement(*element);
}

void SVGMarkerDataBuilder::PathIsDone() {
  float angle = clampTo<float>(CurrentAngle(kEndMarker));
  positions_.push_back(MarkerPosition(kEndMarker, origin_, angle));
}

double SVGMarkerDataBuilder::CurrentAngle(SVGMarkerType type) const {
  // For details of this calculation, see:
  // http://www.w3.org/TR/SVG/single-page.html#painting-MarkerElement
  double in_angle = rad2deg(FloatPoint(in_slope_).SlopeAngleRadians());
  double out_angle = rad2deg(FloatPoint(out_slope_).SlopeAngleRadians());

  switch (type) {
    case kStartMarker:
      return out_angle;
    case kMidMarker:
      return BisectingAngle(in_angle, out_angle);
    case kEndMarker:
      return in_angle;
  }

  NOTREACHED();
  return 0;
}

void SVGMarkerDataBuilder::ComputeQuadTangents(SegmentData& data,
                                               const FloatPoint& start,
                                               const FloatPoint& control,
                                               const FloatPoint& end) {
  data.start_tangent = control - start;
  data.end_tangent = end - control;
  if (data.start_tangent.IsZero())
    data.start_tangent = data.end_tangent;
  else if (data.end_tangent.IsZero())
    data.end_tangent = data.start_tangent;
}

SVGMarkerDataBuilder::SegmentData
SVGMarkerDataBuilder::ExtractPathElementFeatures(
    const PathElement& element) const {
  SegmentData data;
  const FloatPoint* points = element.points;
  switch (element.type) {
    case kPathElementAddCurveToPoint:
      data.position = points[2];
      data.start_tangent = points[0] - origin_;
      data.end_tangent = points[2] - points[1];
      if (data.start_tangent.IsZero())
        ComputeQuadTangents(data, points[0], points[1], points[2]);
      else if (data.end_tangent.IsZero())
        ComputeQuadTangents(data, origin_, points[0], points[1]);
      break;
    case kPathElementAddQuadCurveToPoint:
      data.position = points[1];
      ComputeQuadTangents(data, origin_, points[0], points[1]);
      break;
    case kPathElementMoveToPoint:
    case kPathElementAddLineToPoint:
      data.position = points[0];
      data.start_tangent = data.position - origin_;
      data.end_tangent = data.position - origin_;
      break;
    case kPathElementCloseSubpath:
      data.position = subpath_start_;
      data.start_tangent = data.position - origin_;
      data.end_tangent = data.position - origin_;
      break;
  }
  return data;
}

void SVGMarkerDataBuilder::UpdateFromPathElement(const PathElement& element) {
  SegmentData segment_data = ExtractPathElementFeatures(element);

  // First update the outgoing slope for the previous element.
  out_slope_ = segment_data.start_tangent;

  // Record the marker for the previous element.
  if (element_index_ > 0) {
    SVGMarkerType marker_type = element_index_ == 1 ? kStartMarker : kMidMarker;
    float angle = clampTo<float>(CurrentAngle(marker_type));
    positions_.push_back(MarkerPosition(marker_type, origin_, angle));
  }

  // Update the incoming slope for this marker position.
  in_slope_ = segment_data.end_tangent;

  // Update marker position.
  origin_ = segment_data.position;

  // If this is a 'move to' segment, save the point for use with 'close'.
  if (element.type == kPathElementMoveToPoint)
    subpath_start_ = element.points[0];
  else if (element.type == kPathElementCloseSubpath)
    subpath_start_ = FloatPoint();

  ++element_index_;
}

}  // namespace blink
