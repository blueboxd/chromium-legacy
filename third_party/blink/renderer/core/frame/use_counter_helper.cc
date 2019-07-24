/*
 * Copyright (C) 2012 Google, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/frame/use_counter_helper.h"

#include "third_party/blink/public/mojom/use_counter/css_property_id.mojom-blink.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"

namespace blink {

UseCounterMuteScope::UseCounterMuteScope(const Element& element)
    : loader_(element.GetDocument().Loader()) {
  if (loader_)
    loader_->GetUseCounterHelper().MuteForInspector();
}

UseCounterMuteScope::~UseCounterMuteScope() {
  if (loader_)
    loader_->GetUseCounterHelper().UnmuteForInspector();
}

// TODO(loonybear): Move CSSPropertyID to
// public/mojom/use_counter/css_property_id.mojom to plumb CSS metrics end to
// end to PageLoadMetrics.
int UseCounterHelper::MapCSSPropertyIdToCSSSampleIdForHistogram(
    CSSPropertyID unresolved_property) {
  switch (unresolved_property) {
    // Begin at 2, because 1 is reserved for totalPagesMeasuredCSSSampleId.
    case CSSPropertyID::kColor:
      return 2;
    case CSSPropertyID::kDirection:
      return 3;
    case CSSPropertyID::kDisplay:
      return 4;
    case CSSPropertyID::kFont:
      return 5;
    case CSSPropertyID::kFontFamily:
      return 6;
    case CSSPropertyID::kFontSize:
      return 7;
    case CSSPropertyID::kFontStyle:
      return 8;
    case CSSPropertyID::kFontVariant:
      return 9;
    case CSSPropertyID::kFontWeight:
      return 10;
    case CSSPropertyID::kTextRendering:
      return 11;
    case CSSPropertyID::kAliasWebkitFontFeatureSettings:
      return 12;
    case CSSPropertyID::kFontKerning:
      return 13;
    case CSSPropertyID::kWebkitFontSmoothing:
      return 14;
    case CSSPropertyID::kFontVariantLigatures:
      return 15;
    case CSSPropertyID::kWebkitLocale:
      return 16;
    case CSSPropertyID::kWebkitTextOrientation:
      return 17;
    case CSSPropertyID::kWebkitWritingMode:
      return 18;
    case CSSPropertyID::kZoom:
      return 19;
    case CSSPropertyID::kLineHeight:
      return 20;
    case CSSPropertyID::kBackground:
      return 21;
    case CSSPropertyID::kBackgroundAttachment:
      return 22;
    case CSSPropertyID::kBackgroundClip:
      return 23;
    case CSSPropertyID::kBackgroundColor:
      return 24;
    case CSSPropertyID::kBackgroundImage:
      return 25;
    case CSSPropertyID::kBackgroundOrigin:
      return 26;
    case CSSPropertyID::kBackgroundPosition:
      return 27;
    case CSSPropertyID::kBackgroundPositionX:
      return 28;
    case CSSPropertyID::kBackgroundPositionY:
      return 29;
    case CSSPropertyID::kBackgroundRepeat:
      return 30;
    case CSSPropertyID::kBackgroundRepeatX:
      return 31;
    case CSSPropertyID::kBackgroundRepeatY:
      return 32;
    case CSSPropertyID::kBackgroundSize:
      return 33;
    case CSSPropertyID::kBorder:
      return 34;
    case CSSPropertyID::kBorderBottom:
      return 35;
    case CSSPropertyID::kBorderBottomColor:
      return 36;
    case CSSPropertyID::kBorderBottomLeftRadius:
      return 37;
    case CSSPropertyID::kBorderBottomRightRadius:
      return 38;
    case CSSPropertyID::kBorderBottomStyle:
      return 39;
    case CSSPropertyID::kBorderBottomWidth:
      return 40;
    case CSSPropertyID::kBorderCollapse:
      return 41;
    case CSSPropertyID::kBorderColor:
      return 42;
    case CSSPropertyID::kBorderImage:
      return 43;
    case CSSPropertyID::kBorderImageOutset:
      return 44;
    case CSSPropertyID::kBorderImageRepeat:
      return 45;
    case CSSPropertyID::kBorderImageSlice:
      return 46;
    case CSSPropertyID::kBorderImageSource:
      return 47;
    case CSSPropertyID::kBorderImageWidth:
      return 48;
    case CSSPropertyID::kBorderLeft:
      return 49;
    case CSSPropertyID::kBorderLeftColor:
      return 50;
    case CSSPropertyID::kBorderLeftStyle:
      return 51;
    case CSSPropertyID::kBorderLeftWidth:
      return 52;
    case CSSPropertyID::kBorderRadius:
      return 53;
    case CSSPropertyID::kBorderRight:
      return 54;
    case CSSPropertyID::kBorderRightColor:
      return 55;
    case CSSPropertyID::kBorderRightStyle:
      return 56;
    case CSSPropertyID::kBorderRightWidth:
      return 57;
    case CSSPropertyID::kBorderSpacing:
      return 58;
    case CSSPropertyID::kBorderStyle:
      return 59;
    case CSSPropertyID::kBorderTop:
      return 60;
    case CSSPropertyID::kBorderTopColor:
      return 61;
    case CSSPropertyID::kBorderTopLeftRadius:
      return 62;
    case CSSPropertyID::kBorderTopRightRadius:
      return 63;
    case CSSPropertyID::kBorderTopStyle:
      return 64;
    case CSSPropertyID::kBorderTopWidth:
      return 65;
    case CSSPropertyID::kBorderWidth:
      return 66;
    case CSSPropertyID::kBottom:
      return 67;
    case CSSPropertyID::kBoxShadow:
      return 68;
    case CSSPropertyID::kBoxSizing:
      return 69;
    case CSSPropertyID::kCaptionSide:
      return 70;
    case CSSPropertyID::kClear:
      return 71;
    case CSSPropertyID::kClip:
      return 72;
    case CSSPropertyID::kAliasWebkitClipPath:
      return 73;
    case CSSPropertyID::kContent:
      return 74;
    case CSSPropertyID::kCounterIncrement:
      return 75;
    case CSSPropertyID::kCounterReset:
      return 76;
    case CSSPropertyID::kCursor:
      return 77;
    case CSSPropertyID::kEmptyCells:
      return 78;
    case CSSPropertyID::kFloat:
      return 79;
    case CSSPropertyID::kFontStretch:
      return 80;
    case CSSPropertyID::kHeight:
      return 81;
    case CSSPropertyID::kImageRendering:
      return 82;
    case CSSPropertyID::kLeft:
      return 83;
    case CSSPropertyID::kLetterSpacing:
      return 84;
    case CSSPropertyID::kListStyle:
      return 85;
    case CSSPropertyID::kListStyleImage:
      return 86;
    case CSSPropertyID::kListStylePosition:
      return 87;
    case CSSPropertyID::kListStyleType:
      return 88;
    case CSSPropertyID::kMargin:
      return 89;
    case CSSPropertyID::kMarginBottom:
      return 90;
    case CSSPropertyID::kMarginLeft:
      return 91;
    case CSSPropertyID::kMarginRight:
      return 92;
    case CSSPropertyID::kMarginTop:
      return 93;
    case CSSPropertyID::kMaxHeight:
      return 94;
    case CSSPropertyID::kMaxWidth:
      return 95;
    case CSSPropertyID::kMinHeight:
      return 96;
    case CSSPropertyID::kMinWidth:
      return 97;
    case CSSPropertyID::kOpacity:
      return 98;
    case CSSPropertyID::kOrphans:
      return 99;
    case CSSPropertyID::kOutline:
      return 100;
    case CSSPropertyID::kOutlineColor:
      return 101;
    case CSSPropertyID::kOutlineOffset:
      return 102;
    case CSSPropertyID::kOutlineStyle:
      return 103;
    case CSSPropertyID::kOutlineWidth:
      return 104;
    case CSSPropertyID::kOverflow:
      return 105;
    case CSSPropertyID::kOverflowWrap:
      return 106;
    case CSSPropertyID::kOverflowX:
      return 107;
    case CSSPropertyID::kOverflowY:
      return 108;
    case CSSPropertyID::kPadding:
      return 109;
    case CSSPropertyID::kPaddingBottom:
      return 110;
    case CSSPropertyID::kPaddingLeft:
      return 111;
    case CSSPropertyID::kPaddingRight:
      return 112;
    case CSSPropertyID::kPaddingTop:
      return 113;
    case CSSPropertyID::kPage:
      return 114;
    case CSSPropertyID::kPageBreakAfter:
      return 115;
    case CSSPropertyID::kPageBreakBefore:
      return 116;
    case CSSPropertyID::kPageBreakInside:
      return 117;
    case CSSPropertyID::kPointerEvents:
      return 118;
    case CSSPropertyID::kPosition:
      return 119;
    case CSSPropertyID::kQuotes:
      return 120;
    case CSSPropertyID::kResize:
      return 121;
    case CSSPropertyID::kRight:
      return 122;
    case CSSPropertyID::kSize:
      return 123;
    case CSSPropertyID::kSrc:
      return 124;
    case CSSPropertyID::kSpeak:
      return 125;
    case CSSPropertyID::kTableLayout:
      return 126;
    case CSSPropertyID::kTabSize:
      return 127;
    case CSSPropertyID::kTextAlign:
      return 128;
    case CSSPropertyID::kTextDecoration:
      return 129;
    case CSSPropertyID::kTextIndent:
      return 130;
    /* Removed CSSPropertyTextLineThrough* - 131-135 */
    case CSSPropertyID::kTextOverflow:
      return 136;
    /* Removed CSSPropertyTextOverline* - 137-141 */
    case CSSPropertyID::kTextShadow:
      return 142;
    case CSSPropertyID::kTextTransform:
      return 143;
    /* Removed CSSPropertyTextUnderline* - 144-148 */
    case CSSPropertyID::kTop:
      return 149;
    case CSSPropertyID::kTransition:
      return 150;
    case CSSPropertyID::kTransitionDelay:
      return 151;
    case CSSPropertyID::kTransitionDuration:
      return 152;
    case CSSPropertyID::kTransitionProperty:
      return 153;
    case CSSPropertyID::kTransitionTimingFunction:
      return 154;
    case CSSPropertyID::kUnicodeBidi:
      return 155;
    case CSSPropertyID::kUnicodeRange:
      return 156;
    case CSSPropertyID::kVerticalAlign:
      return 157;
    case CSSPropertyID::kVisibility:
      return 158;
    case CSSPropertyID::kWhiteSpace:
      return 159;
    case CSSPropertyID::kWidows:
      return 160;
    case CSSPropertyID::kWidth:
      return 161;
    case CSSPropertyID::kWordBreak:
      return 162;
    case CSSPropertyID::kWordSpacing:
      return 163;
    case CSSPropertyID::kAliasWordWrap:
      return 164;
    case CSSPropertyID::kZIndex:
      return 165;
    case CSSPropertyID::kAliasWebkitAnimation:
      return 166;
    case CSSPropertyID::kAliasWebkitAnimationDelay:
      return 167;
    case CSSPropertyID::kAliasWebkitAnimationDirection:
      return 168;
    case CSSPropertyID::kAliasWebkitAnimationDuration:
      return 169;
    case CSSPropertyID::kAliasWebkitAnimationFillMode:
      return 170;
    case CSSPropertyID::kAliasWebkitAnimationIterationCount:
      return 171;
    case CSSPropertyID::kAliasWebkitAnimationName:
      return 172;
    case CSSPropertyID::kAliasWebkitAnimationPlayState:
      return 173;
    case CSSPropertyID::kAliasWebkitAnimationTimingFunction:
      return 174;
    case CSSPropertyID::kWebkitAppearance:
      return 175;
    // CSSPropertyWebkitAspectRatio was 176
    case CSSPropertyID::kAliasWebkitBackfaceVisibility:
      return 177;
    case CSSPropertyID::kAliasWebkitBackgroundClip:
      return 178;
    // case CSSPropertyWebkitBackgroundComposite: return 179;
    case CSSPropertyID::kAliasWebkitBackgroundOrigin:
      return 180;
    case CSSPropertyID::kAliasWebkitBackgroundSize:
      return 181;
    case CSSPropertyID::kAliasWebkitBorderAfter:
      return 182;
    case CSSPropertyID::kAliasWebkitBorderAfterColor:
      return 183;
    case CSSPropertyID::kAliasWebkitBorderAfterStyle:
      return 184;
    case CSSPropertyID::kAliasWebkitBorderAfterWidth:
      return 185;
    case CSSPropertyID::kAliasWebkitBorderBefore:
      return 186;
    case CSSPropertyID::kAliasWebkitBorderBeforeColor:
      return 187;
    case CSSPropertyID::kAliasWebkitBorderBeforeStyle:
      return 188;
    case CSSPropertyID::kAliasWebkitBorderBeforeWidth:
      return 189;
    case CSSPropertyID::kAliasWebkitBorderEnd:
      return 190;
    case CSSPropertyID::kAliasWebkitBorderEndColor:
      return 191;
    case CSSPropertyID::kAliasWebkitBorderEndStyle:
      return 192;
    case CSSPropertyID::kAliasWebkitBorderEndWidth:
      return 193;
    // CSSPropertyWebkitBorderFit was 194
    case CSSPropertyID::kWebkitBorderHorizontalSpacing:
      return 195;
    case CSSPropertyID::kWebkitBorderImage:
      return 196;
    case CSSPropertyID::kAliasWebkitBorderRadius:
      return 197;
    case CSSPropertyID::kAliasWebkitBorderStart:
      return 198;
    case CSSPropertyID::kAliasWebkitBorderStartColor:
      return 199;
    case CSSPropertyID::kAliasWebkitBorderStartStyle:
      return 200;
    case CSSPropertyID::kAliasWebkitBorderStartWidth:
      return 201;
    case CSSPropertyID::kWebkitBorderVerticalSpacing:
      return 202;
    case CSSPropertyID::kWebkitBoxAlign:
      return 203;
    case CSSPropertyID::kWebkitBoxDirection:
      return 204;
    case CSSPropertyID::kWebkitBoxFlex:
      return 205;
    // CSSPropertyWebkitBoxFlexGroup was 206
    // CSSPropertyWebkitBoxLines was 207
    case CSSPropertyID::kWebkitBoxOrdinalGroup:
      return 208;
    case CSSPropertyID::kWebkitBoxOrient:
      return 209;
    case CSSPropertyID::kWebkitBoxPack:
      return 210;
    case CSSPropertyID::kWebkitBoxReflect:
      return 211;
    case CSSPropertyID::kAliasWebkitBoxShadow:
      return 212;
    // CSSPropertyWebkitColumnAxis was 214
    case CSSPropertyID::kWebkitColumnBreakAfter:
      return 215;
    case CSSPropertyID::kWebkitColumnBreakBefore:
      return 216;
    case CSSPropertyID::kWebkitColumnBreakInside:
      return 217;
    case CSSPropertyID::kAliasWebkitColumnCount:
      return 218;
    case CSSPropertyID::kAliasWebkitColumnGap:
      return 219;
    // CSSPropertyWebkitColumnProgression was 220
    case CSSPropertyID::kAliasWebkitColumnRule:
      return 221;
    case CSSPropertyID::kAliasWebkitColumnRuleColor:
      return 222;
    case CSSPropertyID::kAliasWebkitColumnRuleStyle:
      return 223;
    case CSSPropertyID::kAliasWebkitColumnRuleWidth:
      return 224;
    case CSSPropertyID::kAliasWebkitColumnSpan:
      return 225;
    case CSSPropertyID::kAliasWebkitColumnWidth:
      return 226;
    case CSSPropertyID::kAliasWebkitColumns:
      return 227;
    // 228 was CSSPropertyID::kWebkitBoxDecorationBreak (duplicated due to
    // #ifdef). 229 was CSSPropertyWebkitFilter (duplicated due to #ifdef).
    case CSSPropertyID::kAlignContent:
      return 230;
    case CSSPropertyID::kAlignItems:
      return 231;
    case CSSPropertyID::kAlignSelf:
      return 232;
    case CSSPropertyID::kFlex:
      return 233;
    case CSSPropertyID::kFlexBasis:
      return 234;
    case CSSPropertyID::kFlexDirection:
      return 235;
    case CSSPropertyID::kFlexFlow:
      return 236;
    case CSSPropertyID::kFlexGrow:
      return 237;
    case CSSPropertyID::kFlexShrink:
      return 238;
    case CSSPropertyID::kFlexWrap:
      return 239;
    case CSSPropertyID::kJustifyContent:
      return 240;
    case CSSPropertyID::kWebkitFontSizeDelta:
      return 241;
    case CSSPropertyID::kGridTemplateColumns:
      return 242;
    case CSSPropertyID::kGridTemplateRows:
      return 243;
    case CSSPropertyID::kGridColumnStart:
      return 244;
    case CSSPropertyID::kGridColumnEnd:
      return 245;
    case CSSPropertyID::kGridRowStart:
      return 246;
    case CSSPropertyID::kGridRowEnd:
      return 247;
    case CSSPropertyID::kGridColumn:
      return 248;
    case CSSPropertyID::kGridRow:
      return 249;
    case CSSPropertyID::kGridAutoFlow:
      return 250;
    case CSSPropertyID::kWebkitHighlight:
      return 251;
    case CSSPropertyID::kWebkitHyphenateCharacter:
      return 252;
    // case CSSPropertyWebkitLineBoxContain: return 257;
    // case CSSPropertyWebkitLineAlign: return 258;
    case CSSPropertyID::kWebkitLineBreak:
      return 259;
    case CSSPropertyID::kWebkitLineClamp:
      return 260;
    // case CSSPropertyWebkitLineGrid: return 261;
    // case CSSPropertyWebkitLineSnap: return 262;
    case CSSPropertyID::kAliasWebkitLogicalWidth:
      return 263;
    case CSSPropertyID::kAliasWebkitLogicalHeight:
      return 264;
    case CSSPropertyID::kWebkitMarginAfterCollapse:
      return 265;
    case CSSPropertyID::kWebkitMarginBeforeCollapse:
      return 266;
    case CSSPropertyID::kWebkitMarginBottomCollapse:
      return 267;
    case CSSPropertyID::kWebkitMarginTopCollapse:
      return 268;
    case CSSPropertyID::kWebkitMarginCollapse:
      return 269;
    case CSSPropertyID::kAliasWebkitMarginAfter:
      return 270;
    case CSSPropertyID::kAliasWebkitMarginBefore:
      return 271;
    case CSSPropertyID::kAliasWebkitMarginEnd:
      return 272;
    case CSSPropertyID::kAliasWebkitMarginStart:
      return 273;
    // CSSPropertyWebkitMarquee was 274.
    // CSSPropertyInternalMarquee* were 275-279.
    case CSSPropertyID::kWebkitMask:
      return 280;
    case CSSPropertyID::kWebkitMaskBoxImage:
      return 281;
    case CSSPropertyID::kWebkitMaskBoxImageOutset:
      return 282;
    case CSSPropertyID::kWebkitMaskBoxImageRepeat:
      return 283;
    case CSSPropertyID::kWebkitMaskBoxImageSlice:
      return 284;
    case CSSPropertyID::kWebkitMaskBoxImageSource:
      return 285;
    case CSSPropertyID::kWebkitMaskBoxImageWidth:
      return 286;
    case CSSPropertyID::kWebkitMaskClip:
      return 287;
    case CSSPropertyID::kWebkitMaskComposite:
      return 288;
    case CSSPropertyID::kWebkitMaskImage:
      return 289;
    case CSSPropertyID::kWebkitMaskOrigin:
      return 290;
    case CSSPropertyID::kWebkitMaskPosition:
      return 291;
    case CSSPropertyID::kWebkitMaskPositionX:
      return 292;
    case CSSPropertyID::kWebkitMaskPositionY:
      return 293;
    case CSSPropertyID::kWebkitMaskRepeat:
      return 294;
    case CSSPropertyID::kWebkitMaskRepeatX:
      return 295;
    case CSSPropertyID::kWebkitMaskRepeatY:
      return 296;
    case CSSPropertyID::kWebkitMaskSize:
      return 297;
    case CSSPropertyID::kAliasWebkitMaxLogicalWidth:
      return 298;
    case CSSPropertyID::kAliasWebkitMaxLogicalHeight:
      return 299;
    case CSSPropertyID::kAliasWebkitMinLogicalWidth:
      return 300;
    case CSSPropertyID::kAliasWebkitMinLogicalHeight:
      return 301;
    // WebkitNbspMode has been deleted, was return 302;
    case CSSPropertyID::kOrder:
      return 303;
    case CSSPropertyID::kAliasWebkitPaddingAfter:
      return 304;
    case CSSPropertyID::kAliasWebkitPaddingBefore:
      return 305;
    case CSSPropertyID::kAliasWebkitPaddingEnd:
      return 306;
    case CSSPropertyID::kAliasWebkitPaddingStart:
      return 307;
    case CSSPropertyID::kAliasWebkitPerspective:
      return 308;
    case CSSPropertyID::kAliasWebkitPerspectiveOrigin:
      return 309;
    case CSSPropertyID::kWebkitPerspectiveOriginX:
      return 310;
    case CSSPropertyID::kWebkitPerspectiveOriginY:
      return 311;
    case CSSPropertyID::kWebkitPrintColorAdjust:
      return 312;
    case CSSPropertyID::kWebkitRtlOrdering:
      return 313;
    case CSSPropertyID::kWebkitRubyPosition:
      return 314;
    case CSSPropertyID::kWebkitTextCombine:
      return 315;
    case CSSPropertyID::kWebkitTextDecorationsInEffect:
      return 316;
    case CSSPropertyID::kWebkitTextEmphasis:
      return 317;
    case CSSPropertyID::kWebkitTextEmphasisColor:
      return 318;
    case CSSPropertyID::kWebkitTextEmphasisPosition:
      return 319;
    case CSSPropertyID::kWebkitTextEmphasisStyle:
      return 320;
    case CSSPropertyID::kWebkitTextFillColor:
      return 321;
    case CSSPropertyID::kWebkitTextSecurity:
      return 322;
    case CSSPropertyID::kWebkitTextStroke:
      return 323;
    case CSSPropertyID::kWebkitTextStrokeColor:
      return 324;
    case CSSPropertyID::kWebkitTextStrokeWidth:
      return 325;
    case CSSPropertyID::kAliasWebkitTransform:
      return 326;
    case CSSPropertyID::kAliasWebkitTransformOrigin:
      return 327;
    case CSSPropertyID::kWebkitTransformOriginX:
      return 328;
    case CSSPropertyID::kWebkitTransformOriginY:
      return 329;
    case CSSPropertyID::kWebkitTransformOriginZ:
      return 330;
    case CSSPropertyID::kAliasWebkitTransformStyle:
      return 331;
    case CSSPropertyID::kAliasWebkitTransition:
      return 332;
    case CSSPropertyID::kAliasWebkitTransitionDelay:
      return 333;
    case CSSPropertyID::kAliasWebkitTransitionDuration:
      return 334;
    case CSSPropertyID::kAliasWebkitTransitionProperty:
      return 335;
    case CSSPropertyID::kAliasWebkitTransitionTimingFunction:
      return 336;
    case CSSPropertyID::kWebkitUserDrag:
      return 337;
    case CSSPropertyID::kWebkitUserModify:
      return 338;
    case CSSPropertyID::kAliasWebkitUserSelect:
      return 339;
    // case CSSPropertyWebkitFlowInto: return 340;
    // case CSSPropertyWebkitFlowFrom: return 341;
    // case CSSPropertyWebkitRegionFragment: return 342;
    // case CSSPropertyWebkitRegionBreakAfter: return 343;
    // case CSSPropertyWebkitRegionBreakBefore: return 344;
    // case CSSPropertyWebkitRegionBreakInside: return 345;
    // case CSSPropertyShapeInside: return 346;
    case CSSPropertyID::kShapeOutside:
      return 347;
    case CSSPropertyID::kShapeMargin:
      return 348;
    // case CSSPropertyShapePadding: return 349;
    // case CSSPropertyWebkitWrapFlow: return 350;
    // case CSSPropertyWebkitWrapThrough: return 351;
    // CSSPropertyWebkitWrap was 352.
    // 353 was CSSPropertyID::kWebkitTapHighlightColor (duplicated due to
    // #ifdef). 354 was CSSPropertyID::kWebkitAppRegion (duplicated due to
    // #ifdef).
    case CSSPropertyID::kClipPath:
      return 355;
    case CSSPropertyID::kClipRule:
      return 356;
    case CSSPropertyID::kMask:
      return 357;
    // CSSPropertyEnableBackground has been removed, was return 358;
    case CSSPropertyID::kFilter:
      return 359;
    case CSSPropertyID::kFloodColor:
      return 360;
    case CSSPropertyID::kFloodOpacity:
      return 361;
    case CSSPropertyID::kLightingColor:
      return 362;
    case CSSPropertyID::kStopColor:
      return 363;
    case CSSPropertyID::kStopOpacity:
      return 364;
    case CSSPropertyID::kColorInterpolation:
      return 365;
    case CSSPropertyID::kColorInterpolationFilters:
      return 366;
    // case CSSPropertyColorProfile: return 367;
    case CSSPropertyID::kColorRendering:
      return 368;
    case CSSPropertyID::kFill:
      return 369;
    case CSSPropertyID::kFillOpacity:
      return 370;
    case CSSPropertyID::kFillRule:
      return 371;
    case CSSPropertyID::kMarker:
      return 372;
    case CSSPropertyID::kMarkerEnd:
      return 373;
    case CSSPropertyID::kMarkerMid:
      return 374;
    case CSSPropertyID::kMarkerStart:
      return 375;
    case CSSPropertyID::kMaskType:
      return 376;
    case CSSPropertyID::kShapeRendering:
      return 377;
    case CSSPropertyID::kStroke:
      return 378;
    case CSSPropertyID::kStrokeDasharray:
      return 379;
    case CSSPropertyID::kStrokeDashoffset:
      return 380;
    case CSSPropertyID::kStrokeLinecap:
      return 381;
    case CSSPropertyID::kStrokeLinejoin:
      return 382;
    case CSSPropertyID::kStrokeMiterlimit:
      return 383;
    case CSSPropertyID::kStrokeOpacity:
      return 384;
    case CSSPropertyID::kStrokeWidth:
      return 385;
    case CSSPropertyID::kAlignmentBaseline:
      return 386;
    case CSSPropertyID::kBaselineShift:
      return 387;
    case CSSPropertyID::kDominantBaseline:
      return 388;
    // CSSPropertyGlyphOrientationHorizontal has been removed, was return 389;
    // CSSPropertyGlyphOrientationVertical has been removed, was return 390;
    // CSSPropertyKerning has been removed, was return 391;
    case CSSPropertyID::kTextAnchor:
      return 392;
    case CSSPropertyID::kVectorEffect:
      return 393;
    case CSSPropertyID::kWritingMode:
      return 394;
// CSSPropertyWebkitSvgShadow has been removed, was return 395;
// CSSPropertyWebkitCursorVisibility has been removed, was return 396;
// CSSPropertyID::kImageOrientation has been removed, was return 397;
// CSSPropertyImageResolution has been removed, was return 398;
#if defined(ENABLE_CSS_COMPOSITING) && ENABLE_CSS_COMPOSITING
    case CSSPropertyWebkitBlendMode:
      return 399;
    case CSSPropertyWebkitBackgroundBlendMode:
      return 400;
#endif
    case CSSPropertyID::kTextDecorationLine:
      return 401;
    case CSSPropertyID::kTextDecorationStyle:
      return 402;
    case CSSPropertyID::kTextDecorationColor:
      return 403;
    case CSSPropertyID::kTextAlignLast:
      return 404;
    case CSSPropertyID::kTextUnderlinePosition:
      return 405;
    case CSSPropertyID::kMaxZoom:
      return 406;
    case CSSPropertyID::kMinZoom:
      return 407;
    case CSSPropertyID::kOrientation:
      return 408;
    case CSSPropertyID::kUserZoom:
      return 409;
    // CSSPropertyWebkitDashboardRegion was 410.
    // CSSPropertyWebkitOverflowScrolling was 411.
    case CSSPropertyID::kWebkitAppRegion:
      return 412;
    case CSSPropertyID::kAliasWebkitFilter:
      return 413;
    case CSSPropertyID::kWebkitBoxDecorationBreak:
      return 414;
    case CSSPropertyID::kWebkitTapHighlightColor:
      return 415;
    case CSSPropertyID::kBufferedRendering:
      return 416;
    case CSSPropertyID::kGridAutoRows:
      return 417;
    case CSSPropertyID::kGridAutoColumns:
      return 418;
    case CSSPropertyID::kBackgroundBlendMode:
      return 419;
    case CSSPropertyID::kMixBlendMode:
      return 420;
    case CSSPropertyID::kTouchAction:
      return 421;
    case CSSPropertyID::kGridArea:
      return 422;
    case CSSPropertyID::kGridTemplateAreas:
      return 423;
    case CSSPropertyID::kAnimation:
      return 424;
    case CSSPropertyID::kAnimationDelay:
      return 425;
    case CSSPropertyID::kAnimationDirection:
      return 426;
    case CSSPropertyID::kAnimationDuration:
      return 427;
    case CSSPropertyID::kAnimationFillMode:
      return 428;
    case CSSPropertyID::kAnimationIterationCount:
      return 429;
    case CSSPropertyID::kAnimationName:
      return 430;
    case CSSPropertyID::kAnimationPlayState:
      return 431;
    case CSSPropertyID::kAnimationTimingFunction:
      return 432;
    case CSSPropertyID::kObjectFit:
      return 433;
    case CSSPropertyID::kPaintOrder:
      return 434;
    case CSSPropertyID::kMaskSourceType:
      return 435;
    case CSSPropertyID::kIsolation:
      return 436;
    case CSSPropertyID::kObjectPosition:
      return 437;
    // case CSSPropertyInternalCallback: return 438;
    case CSSPropertyID::kShapeImageThreshold:
      return 439;
    case CSSPropertyID::kColumnFill:
      return 440;
    case CSSPropertyID::kTextJustify:
      return 441;
    // CSSPropertyTouchActionDelay was 442
    case CSSPropertyID::kJustifySelf:
      return 443;
    case CSSPropertyID::kScrollBehavior:
      return 444;
    case CSSPropertyID::kWillChange:
      return 445;
    case CSSPropertyID::kTransform:
      return 446;
    case CSSPropertyID::kTransformOrigin:
      return 447;
    case CSSPropertyID::kTransformStyle:
      return 448;
    case CSSPropertyID::kPerspective:
      return 449;
    case CSSPropertyID::kPerspectiveOrigin:
      return 450;
    case CSSPropertyID::kBackfaceVisibility:
      return 451;
    case CSSPropertyID::kGridTemplate:
      return 452;
    case CSSPropertyID::kGrid:
      return 453;
    case CSSPropertyID::kAll:
      return 454;
    case CSSPropertyID::kJustifyItems:
      return 455;
    // CSSPropertyMotionPath was 457.
    // CSSPropertyAliasMotionOffset was 458.
    // CSSPropertyAliasMotionRotation was 459.
    // CSSPropertyMotion was 460.
    case CSSPropertyID::kX:
      return 461;
    case CSSPropertyID::kY:
      return 462;
    case CSSPropertyID::kRx:
      return 463;
    case CSSPropertyID::kRy:
      return 464;
    case CSSPropertyID::kFontSizeAdjust:
      return 465;
    case CSSPropertyID::kCx:
      return 466;
    case CSSPropertyID::kCy:
      return 467;
    case CSSPropertyID::kR:
      return 468;
    case CSSPropertyID::kAliasEpubCaptionSide:
      return 469;
    case CSSPropertyID::kAliasEpubTextCombine:
      return 470;
    case CSSPropertyID::kAliasEpubTextEmphasis:
      return 471;
    case CSSPropertyID::kAliasEpubTextEmphasisColor:
      return 472;
    case CSSPropertyID::kAliasEpubTextEmphasisStyle:
      return 473;
    case CSSPropertyID::kAliasEpubTextOrientation:
      return 474;
    case CSSPropertyID::kAliasEpubTextTransform:
      return 475;
    case CSSPropertyID::kAliasEpubWordBreak:
      return 476;
    case CSSPropertyID::kAliasEpubWritingMode:
      return 477;
    case CSSPropertyID::kAliasWebkitAlignContent:
      return 478;
    case CSSPropertyID::kAliasWebkitAlignItems:
      return 479;
    case CSSPropertyID::kAliasWebkitAlignSelf:
      return 480;
    case CSSPropertyID::kAliasWebkitBorderBottomLeftRadius:
      return 481;
    case CSSPropertyID::kAliasWebkitBorderBottomRightRadius:
      return 482;
    case CSSPropertyID::kAliasWebkitBorderTopLeftRadius:
      return 483;
    case CSSPropertyID::kAliasWebkitBorderTopRightRadius:
      return 484;
    case CSSPropertyID::kAliasWebkitBoxSizing:
      return 485;
    case CSSPropertyID::kAliasWebkitFlex:
      return 486;
    case CSSPropertyID::kAliasWebkitFlexBasis:
      return 487;
    case CSSPropertyID::kAliasWebkitFlexDirection:
      return 488;
    case CSSPropertyID::kAliasWebkitFlexFlow:
      return 489;
    case CSSPropertyID::kAliasWebkitFlexGrow:
      return 490;
    case CSSPropertyID::kAliasWebkitFlexShrink:
      return 491;
    case CSSPropertyID::kAliasWebkitFlexWrap:
      return 492;
    case CSSPropertyID::kAliasWebkitJustifyContent:
      return 493;
    case CSSPropertyID::kAliasWebkitOpacity:
      return 494;
    case CSSPropertyID::kAliasWebkitOrder:
      return 495;
    case CSSPropertyID::kAliasWebkitShapeImageThreshold:
      return 496;
    case CSSPropertyID::kAliasWebkitShapeMargin:
      return 497;
    case CSSPropertyID::kAliasWebkitShapeOutside:
      return 498;
    case CSSPropertyID::kScrollSnapType:
      return 499;
    // CSSPropertyScrollSnapPointsX was 500.
    // CSSPropertyScrollSnapPointsY was 501.
    // CSSPropertyScrollSnapCoordinate was 502.
    // CSSPropertyScrollSnapDestination was 503.
    case CSSPropertyID::kTranslate:
      return 504;
    case CSSPropertyID::kRotate:
      return 505;
    case CSSPropertyID::kScale:
      return 506;
    case CSSPropertyID::kImageOrientation:
      return 507;
    case CSSPropertyID::kBackdropFilter:
      return 508;
    case CSSPropertyID::kTextCombineUpright:
      return 509;
    case CSSPropertyID::kTextOrientation:
      return 510;
    case CSSPropertyID::kGridColumnGap:
      return 511;
    case CSSPropertyID::kGridRowGap:
      return 512;
    case CSSPropertyID::kGridGap:
      return 513;
    case CSSPropertyID::kFontFeatureSettings:
      return 514;
    case CSSPropertyID::kVariable:
      return 515;
    case CSSPropertyID::kFontDisplay:
      return 516;
    case CSSPropertyID::kContain:
      return 517;
    case CSSPropertyID::kD:
      return 518;
    case CSSPropertyID::kLineHeightStep:
      return 519;
    case CSSPropertyID::kBreakAfter:
      return 520;
    case CSSPropertyID::kBreakBefore:
      return 521;
    case CSSPropertyID::kBreakInside:
      return 522;
    case CSSPropertyID::kColumnCount:
      return 523;
    case CSSPropertyID::kColumnGap:
      return 524;
    case CSSPropertyID::kColumnRule:
      return 525;
    case CSSPropertyID::kColumnRuleColor:
      return 526;
    case CSSPropertyID::kColumnRuleStyle:
      return 527;
    case CSSPropertyID::kColumnRuleWidth:
      return 528;
    case CSSPropertyID::kColumnSpan:
      return 529;
    case CSSPropertyID::kColumnWidth:
      return 530;
    case CSSPropertyID::kColumns:
      return 531;
    // CSSPropertyApplyAtRule was 532.
    case CSSPropertyID::kFontVariantCaps:
      return 533;
    case CSSPropertyID::kHyphens:
      return 534;
    case CSSPropertyID::kFontVariantNumeric:
      return 535;
    case CSSPropertyID::kTextSizeAdjust:
      return 536;
    case CSSPropertyID::kAliasWebkitTextSizeAdjust:
      return 537;
    case CSSPropertyID::kOverflowAnchor:
      return 538;
    case CSSPropertyID::kUserSelect:
      return 539;
    case CSSPropertyID::kOffsetDistance:
      return 540;
    case CSSPropertyID::kOffsetPath:
      return 541;
    // CSSPropertyOffsetRotation was 542.
    case CSSPropertyID::kOffset:
      return 543;
    case CSSPropertyID::kOffsetAnchor:
      return 544;
    case CSSPropertyID::kOffsetPosition:
      return 545;
    // CSSPropertyTextDecorationSkip was 546.
    case CSSPropertyID::kCaretColor:
      return 547;
    case CSSPropertyID::kOffsetRotate:
      return 548;
    case CSSPropertyID::kFontVariationSettings:
      return 549;
    case CSSPropertyID::kInlineSize:
      return 550;
    case CSSPropertyID::kBlockSize:
      return 551;
    case CSSPropertyID::kMinInlineSize:
      return 552;
    case CSSPropertyID::kMinBlockSize:
      return 553;
    case CSSPropertyID::kMaxInlineSize:
      return 554;
    case CSSPropertyID::kMaxBlockSize:
      return 555;
    case CSSPropertyID::kLineBreak:
      return 556;
    case CSSPropertyID::kPlaceContent:
      return 557;
    case CSSPropertyID::kPlaceItems:
      return 558;
    case CSSPropertyID::kTransformBox:
      return 559;
    case CSSPropertyID::kPlaceSelf:
      return 560;
    case CSSPropertyID::kScrollSnapAlign:
      return 561;
    case CSSPropertyID::kScrollPadding:
      return 562;
    case CSSPropertyID::kScrollPaddingTop:
      return 563;
    case CSSPropertyID::kScrollPaddingRight:
      return 564;
    case CSSPropertyID::kScrollPaddingBottom:
      return 565;
    case CSSPropertyID::kScrollPaddingLeft:
      return 566;
    case CSSPropertyID::kScrollPaddingBlock:
      return 567;
    case CSSPropertyID::kScrollPaddingBlockStart:
      return 568;
    case CSSPropertyID::kScrollPaddingBlockEnd:
      return 569;
    case CSSPropertyID::kScrollPaddingInline:
      return 570;
    case CSSPropertyID::kScrollPaddingInlineStart:
      return 571;
    case CSSPropertyID::kScrollPaddingInlineEnd:
      return 572;
    case CSSPropertyID::kScrollMargin:
      return 573;
    case CSSPropertyID::kScrollMarginTop:
      return 574;
    case CSSPropertyID::kScrollMarginRight:
      return 575;
    case CSSPropertyID::kScrollMarginBottom:
      return 576;
    case CSSPropertyID::kScrollMarginLeft:
      return 577;
    case CSSPropertyID::kScrollMarginBlock:
      return 578;
    case CSSPropertyID::kScrollMarginBlockStart:
      return 579;
    case CSSPropertyID::kScrollMarginBlockEnd:
      return 580;
    case CSSPropertyID::kScrollMarginInline:
      return 581;
    case CSSPropertyID::kScrollMarginInlineStart:
      return 582;
    case CSSPropertyID::kScrollMarginInlineEnd:
      return 583;
    case CSSPropertyID::kScrollSnapStop:
      return 584;
    case CSSPropertyID::kOverscrollBehavior:
      return 585;
    case CSSPropertyID::kOverscrollBehaviorX:
      return 586;
    case CSSPropertyID::kOverscrollBehaviorY:
      return 587;
    case CSSPropertyID::kFontVariantEastAsian:
      return 588;
    case CSSPropertyID::kTextDecorationSkipInk:
      return 589;
    case CSSPropertyID::kScrollCustomization:
      return 590;
    case CSSPropertyID::kRowGap:
      return 591;
    case CSSPropertyID::kGap:
      return 592;
    case CSSPropertyID::kViewportFit:
      return 593;
    case CSSPropertyID::kMarginBlockStart:
      return 594;
    case CSSPropertyID::kMarginBlockEnd:
      return 595;
    case CSSPropertyID::kMarginInlineStart:
      return 596;
    case CSSPropertyID::kMarginInlineEnd:
      return 597;
    case CSSPropertyID::kPaddingBlockStart:
      return 598;
    case CSSPropertyID::kPaddingBlockEnd:
      return 599;
    case CSSPropertyID::kPaddingInlineStart:
      return 600;
    case CSSPropertyID::kPaddingInlineEnd:
      return 601;
    case CSSPropertyID::kBorderBlockEndColor:
      return 602;
    case CSSPropertyID::kBorderBlockEndStyle:
      return 603;
    case CSSPropertyID::kBorderBlockEndWidth:
      return 604;
    case CSSPropertyID::kBorderBlockStartColor:
      return 605;
    case CSSPropertyID::kBorderBlockStartStyle:
      return 606;
    case CSSPropertyID::kBorderBlockStartWidth:
      return 607;
    case CSSPropertyID::kBorderInlineEndColor:
      return 608;
    case CSSPropertyID::kBorderInlineEndStyle:
      return 609;
    case CSSPropertyID::kBorderInlineEndWidth:
      return 610;
    case CSSPropertyID::kBorderInlineStartColor:
      return 611;
    case CSSPropertyID::kBorderInlineStartStyle:
      return 612;
    case CSSPropertyID::kBorderInlineStartWidth:
      return 613;
    case CSSPropertyID::kBorderBlockStart:
      return 614;
    case CSSPropertyID::kBorderBlockEnd:
      return 615;
    case CSSPropertyID::kBorderInlineStart:
      return 616;
    case CSSPropertyID::kBorderInlineEnd:
      return 617;
    case CSSPropertyID::kMarginBlock:
      return 618;
    case CSSPropertyID::kMarginInline:
      return 619;
    case CSSPropertyID::kPaddingBlock:
      return 620;
    case CSSPropertyID::kPaddingInline:
      return 621;
    case CSSPropertyID::kBorderBlockColor:
      return 622;
    case CSSPropertyID::kBorderBlockStyle:
      return 623;
    case CSSPropertyID::kBorderBlockWidth:
      return 624;
    case CSSPropertyID::kBorderInlineColor:
      return 625;
    case CSSPropertyID::kBorderInlineStyle:
      return 626;
    case CSSPropertyID::kBorderInlineWidth:
      return 627;
    case CSSPropertyID::kBorderBlock:
      return 628;
    case CSSPropertyID::kBorderInline:
      return 629;
    case CSSPropertyID::kInsetBlockStart:
      return 630;
    case CSSPropertyID::kInsetBlockEnd:
      return 631;
    case CSSPropertyID::kInsetBlock:
      return 632;
    case CSSPropertyID::kInsetInlineStart:
      return 633;
    case CSSPropertyID::kInsetInlineEnd:
      return 634;
    case CSSPropertyID::kInsetInline:
      return 635;
    case CSSPropertyID::kInset:
      return 636;
    case CSSPropertyID::kColorScheme:
      return 637;
    case CSSPropertyID::kOverflowInline:
      return 638;
    case CSSPropertyID::kOverflowBlock:
      return 639;
    case CSSPropertyID::kForcedColorAdjust:
      return 640;
    case CSSPropertyID::kInherits:
      return 641;
    case CSSPropertyID::kInitialValue:
      return 642;
    case CSSPropertyID::kSyntax:
      return 643;
    case CSSPropertyID::kOverscrollBehaviorInline:
      return 644;
    case CSSPropertyID::kOverscrollBehaviorBlock:
      return 645;
    // 1. Add new features above this line (don't change the assigned numbers of
    // the existing items).
    // 2. Update kMaximumCSSSampleId (defined in
    // public/mojom/use_counter/css_property_id.mojom) with the new maximum
    // value.
    // 3. Run the update_use_counter_css.py script in
    // chromium/src/tools/metrics/histograms to update the UMA histogram names.
    case CSSPropertyID::kInternalEffectiveZoom:
    case CSSPropertyID::kInternalVisitedBackgroundColor:
    case CSSPropertyID::kInternalVisitedBorderBlockEndColor:
    case CSSPropertyID::kInternalVisitedBorderBlockStartColor:
    case CSSPropertyID::kInternalVisitedBorderBottomColor:
    case CSSPropertyID::kInternalVisitedBorderInlineEndColor:
    case CSSPropertyID::kInternalVisitedBorderInlineStartColor:
    case CSSPropertyID::kInternalVisitedBorderLeftColor:
    case CSSPropertyID::kInternalVisitedBorderRightColor:
    case CSSPropertyID::kInternalVisitedBorderTopColor:
    case CSSPropertyID::kInternalVisitedCaretColor:
    case CSSPropertyID::kInternalVisitedColor:
    case CSSPropertyID::kInternalVisitedColumnRuleColor:
    case CSSPropertyID::kInternalVisitedFill:
    case CSSPropertyID::kInternalVisitedOutlineColor:
    case CSSPropertyID::kInternalVisitedStroke:
    case CSSPropertyID::kInternalVisitedTextDecorationColor:
    case CSSPropertyID::kInternalVisitedTextEmphasisColor:
    case CSSPropertyID::kInternalVisitedTextFillColor:
    case CSSPropertyID::kInternalVisitedTextStrokeColor:
    case CSSPropertyID::kInvalid:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

UseCounterHelper::UseCounterHelper(Context context, CommitState commit_state)
    : mute_count_(0), context_(context), commit_state_(commit_state) {}

void UseCounterHelper::MuteForInspector() {
  mute_count_++;
}

void UseCounterHelper::UnmuteForInspector() {
  mute_count_--;
}

void UseCounterHelper::RecordMeasurement(WebFeature feature,
                                         const LocalFrame& source_frame) {
  if (mute_count_)
    return;

  // PageDestruction is reserved as a scaling factor.
  DCHECK_NE(WebFeature::kOBSOLETE_PageDestruction, feature);
  DCHECK_NE(WebFeature::kPageVisits, feature);
  DCHECK_GE(WebFeature::kNumberOfFeatures, feature);

  int feature_id = static_cast<int>(feature);
  if (features_recorded_[feature_id])
    return;
  if (commit_state_ >= kCommited)
    ReportAndTraceMeasurementByFeatureId(feature_id, source_frame);

  features_recorded_.set(feature_id);
}

void UseCounterHelper::ReportAndTraceMeasurementByFeatureId(
    int feature_id,
    const LocalFrame& source_frame) {
  if (context_ != kDisabledContext) {
    // Note that HTTPArchive tooling looks specifically for this event -
    // see https://github.com/HTTPArchive/httparchive/issues/59
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("blink.feature_usage"),
                 "FeatureFirstUsed", "feature", feature_id);
    if (context_ != kDefaultContext)
      FeaturesHistogram().Count(feature_id);
    if (LocalFrameClient* client = source_frame.Client())
      client->DidObserveNewFeatureUsage(static_cast<WebFeature>(feature_id));
    NotifyFeatureCounted(static_cast<WebFeature>(feature_id));
  }
}

bool UseCounterHelper::HasRecordedMeasurement(WebFeature feature) const {
  if (mute_count_)
    return false;

  // PageDestruction is reserved as a scaling factor.
  DCHECK_NE(WebFeature::kOBSOLETE_PageDestruction, feature);
  DCHECK_NE(WebFeature::kPageVisits, feature);
  DCHECK_GE(WebFeature::kNumberOfFeatures, feature);

  return features_recorded_[static_cast<size_t>(feature)];
}

void UseCounterHelper::ClearMeasurementForTesting(WebFeature feature) {
  features_recorded_.reset(static_cast<size_t>(feature));
}

void UseCounterHelper::Trace(blink::Visitor* visitor) {
  visitor->Trace(observers_);
}

void UseCounterHelper::DidCommitLoad(const LocalFrame* frame) {
  const KURL url = frame->GetDocument()->Url();
  if (url.ProtocolIs("chrome-extension"))
    context_ = kExtensionContext;
  if (url.ProtocolIs("file"))
    context_ = kFileContext;

  DCHECK_EQ(kPreCommit, commit_state_);
  commit_state_ = kCommited;
  if (!mute_count_) {
    // If any feature was recorded prior to navigation commits, flush to the
    // browser side.
    for (wtf_size_t feature_id = 0; feature_id < features_recorded_.size();
         ++feature_id) {
      if (features_recorded_[feature_id])
        ReportAndTraceMeasurementByFeatureId(feature_id, *frame);
    }
    for (wtf_size_t sample_id = 0; sample_id < css_recorded_.size();
         ++sample_id) {
      if (css_recorded_[sample_id])
        ReportAndTraceMeasurementByCSSSampleId(sample_id, frame, false);
      if (animated_css_recorded_[sample_id])
        ReportAndTraceMeasurementByCSSSampleId(sample_id, frame, true);
    }

    // TODO(loonybear): move extension histogram to the browser side.
    if (context_ == kExtensionContext || context_ == kFileContext) {
      FeaturesHistogram().Count(static_cast<int>(WebFeature::kPageVisits));
    }
  }
}

bool UseCounterHelper::IsCounted(CSSPropertyID unresolved_property,
                                 CSSPropertyType type) const {
  if (unresolved_property == CSSPropertyID::kInvalid) {
    return false;
  }
  switch (type) {
    case CSSPropertyType::kDefault:
      return css_recorded_[MapCSSPropertyIdToCSSSampleIdForHistogram(
          unresolved_property)];
    case CSSPropertyType::kAnimation:
      return animated_css_recorded_[MapCSSPropertyIdToCSSSampleIdForHistogram(
          unresolved_property)];
  }
}

void UseCounterHelper::AddObserver(Observer* observer) {
  DCHECK(!observers_.Contains(observer));
  observers_.insert(observer);
}

void UseCounterHelper::ReportAndTraceMeasurementByCSSSampleId(
    int sample_id,
    const LocalFrame* frame,
    bool is_animated) {
  // Note that HTTPArchive tooling looks specifically for this event - see
  // https://github.com/HTTPArchive/httparchive/issues/59
  if (context_ != kDisabledContext && context_ != kExtensionContext) {
    const char* name = is_animated ? "AnimatedCSSFirstUsed" : "CSSFirstUsed";
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("blink.feature_usage"), name,
                 "feature", sample_id);
    if (frame && frame->Client())
      frame->Client()->DidObserveNewCssPropertyUsage(sample_id, is_animated);
  }
}

void UseCounterHelper::Count(CSSPropertyID property,
                             CSSPropertyType type,
                             const LocalFrame* source_frame) {
  DCHECK(isCSSPropertyIDWithName(property) ||
         property == CSSPropertyID::kVariable);

  if (mute_count_)
    return;

  const int sample_id = MapCSSPropertyIdToCSSSampleIdForHistogram(property);
  switch (type) {
    case CSSPropertyType::kDefault:
      if (css_recorded_[sample_id])
        return;
      if (commit_state_ >= kCommited)
        ReportAndTraceMeasurementByCSSSampleId(sample_id, source_frame, false);

      css_recorded_.set(sample_id);
      break;
    case CSSPropertyType::kAnimation:
      if (animated_css_recorded_[sample_id])
        return;
      if (commit_state_ >= kCommited)
        ReportAndTraceMeasurementByCSSSampleId(sample_id, source_frame, true);

      animated_css_recorded_.set(sample_id);
      break;
  }
}

void UseCounterHelper::Count(WebFeature feature,
                             const LocalFrame* source_frame) {
  if (!source_frame)
    return;
  RecordMeasurement(feature, *source_frame);
}

void UseCounterHelper::NotifyFeatureCounted(WebFeature feature) {
  DCHECK(!mute_count_);
  DCHECK_NE(kDisabledContext, context_);
  HeapHashSet<Member<Observer>> to_be_removed;
  for (auto observer : observers_) {
    if (observer->OnCountFeature(feature))
      to_be_removed.insert(observer);
  }
  observers_.RemoveAll(to_be_removed);
}

EnumerationHistogram& UseCounterHelper::FeaturesHistogram() const {
  DEFINE_STATIC_LOCAL(blink::EnumerationHistogram, extension_histogram,
                      ("Blink.UseCounter.Extensions.Features",
                       static_cast<int32_t>(WebFeature::kNumberOfFeatures)));
  DEFINE_STATIC_LOCAL(blink::EnumerationHistogram, file_histogram,
                      ("Blink.UseCounter.File.Features",
                       static_cast<int32_t>(WebFeature::kNumberOfFeatures)));
  // Track what features/properties have been reported to the browser side
  // histogram.
  switch (context_) {
    case kDefaultContext:
      // The default features histogram is being recorded on the browser side.
      NOTREACHED();
      break;
    case kExtensionContext:
      return extension_histogram;
    case kFileContext:
      return file_histogram;
    case kDisabledContext:
      NOTREACHED();
      break;
  }
  NOTREACHED();
  blink::EnumerationHistogram* null = nullptr;
  return *null;
}

}  // namespace blink
