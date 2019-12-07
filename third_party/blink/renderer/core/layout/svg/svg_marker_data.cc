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

#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

static double BisectingAngle(double in_angle, double out_angle) {
  // WK193015: Prevent bugs due to angles being non-continuous.
  if (fabs(in_angle - out_angle) > 180)
    in_angle += 360;
  return (in_angle + out_angle) / 2;
}

void SVGMarkerDataBuilder::Build(const Path& path) {
  path.Apply(this, SVGMarkerDataBuilder::UpdateFromPathElement);
  if (positions_.IsEmpty())
    return;
  const bool kEndsSubpath = true;
  UpdateAngle(kEndsSubpath);
  // Mark the last marker as 'end'.
  positions_.back().type = kEndMarker;
}

void SVGMarkerDataBuilder::UpdateFromPathElement(void* info,
                                                 const PathElement* element) {
  static_cast<SVGMarkerDataBuilder*>(info)->UpdateFromPathElement(*element);
}

double SVGMarkerDataBuilder::CurrentAngle(AngleType type) const {
  // For details of this calculation, see:
  // http://www.w3.org/TR/SVG/single-page.html#painting-MarkerElement
  double in_angle = rad2deg(FloatPoint(in_slope_).SlopeAngleRadians());
  double out_angle = rad2deg(FloatPoint(out_slope_).SlopeAngleRadians());
  switch (type) {
    case kOutbound:
      return out_angle;
    case kBisecting:
      return BisectingAngle(in_angle, out_angle);
    case kInbound:
      return in_angle;
  }
}

SVGMarkerDataBuilder::AngleType SVGMarkerDataBuilder::DetermineAngleType(
    bool ends_subpath) const {
  // If this is closing the path, (re)compute the angle to be the one bisecting
  // the in-slope of the 'close' and the out-slope of the 'move to'.
  if (last_element_type_ == kPathElementCloseSubpath)
    return kBisecting;
  // If this is the end of an open subpath (closed subpaths handled above),
  // use the in-slope.
  if (ends_subpath)
    return kInbound;
  // If |last_element_type_| is a 'move to', apply the same rule as for a
  // "start" marker. If needed we will backpatch the angle later.
  if (last_element_type_ == kPathElementMoveToPoint)
    return kOutbound;
  // Else use the bisecting angle.
  return kBisecting;
}

void SVGMarkerDataBuilder::UpdateAngle(bool ends_subpath) {
  // When closing a subpath, update the current out-slope to be that of the
  // 'move to' command.
  if (last_element_type_ == kPathElementCloseSubpath)
    out_slope_ = last_moveto_out_slope_;
  AngleType type = DetermineAngleType(ends_subpath);
  float angle = clampTo<float>(CurrentAngle(type));
  // When closing a subpath, backpatch the first marker on that subpath.
  if (last_element_type_ == kPathElementCloseSubpath)
    positions_[last_moveto_index_].angle = angle;
  positions_.back().angle = angle;
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

  // Save the out-slope for the new subpath.
  if (last_element_type_ == kPathElementMoveToPoint)
    last_moveto_out_slope_ = out_slope_;

  // Record the angle for the previous element.
  bool starts_new_subpath = element.type == kPathElementMoveToPoint;
  if (!positions_.IsEmpty())
    UpdateAngle(starts_new_subpath);

  // Update the incoming slope for this marker position.
  in_slope_ = segment_data.end_tangent;

  // Update marker position.
  origin_ = segment_data.position;

  // If this is a 'move to' segment, save the point for use with 'close', and
  // the the index in the list to allow backpatching the angle on 'close'.
  if (starts_new_subpath) {
    subpath_start_ = element.points[0];
    last_moveto_index_ = positions_.size();
  } else if (element.type == kPathElementCloseSubpath) {
    subpath_start_ = FloatPoint();
  }

  last_element_type_ = element.type;

  // Output a marker for this element. The angle will be computed at a later
  // stage. Similarly for 'end' markers the marker type will be updated at a
  // later stage.
  SVGMarkerType marker_type = positions_.IsEmpty() ? kStartMarker : kMidMarker;
  positions_.push_back(MarkerPosition(marker_type, origin_, 0));
}

}  // namespace blink
