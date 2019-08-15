// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_page.h"

#include <math.h>
#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/numerics/math_constants.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "pdf/pdfium/pdfium_api_string_buffer_adapter.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_unsupported_features.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "printing/units.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdf_annot.h"

using printing::ConvertUnitDouble;
using printing::kPointsPerInch;
using printing::kPixelsPerInch;

namespace chrome_pdf {

namespace {

PDFiumPage::IsValidLinkFunction g_is_valid_link_func_for_testing = nullptr;

// If the link cannot be converted to a pp::Var, then it is not possible to
// pass it to JS. In this case, ignore the link like other PDF viewers.
// See https://crbug.com/312882 for an example.
// TODO(crbug.com/702993): Get rid of the PPAPI usage here, as well as
// SetIsValidLinkFunctionForTesting() and related code.
bool IsValidLink(const std::string& url) {
  return pp::Var(url).is_string();
}

pp::FloatRect FloatPageRectToPixelRect(FPDF_PAGE page,
                                       const pp::FloatRect& input) {
  int output_width = FPDF_GetPageWidth(page);
  int output_height = FPDF_GetPageHeight(page);

  int min_x;
  int min_y;
  int max_x;
  int max_y;
  FPDF_BOOL ret = FPDF_PageToDevice(page, 0, 0, output_width, output_height, 0,
                                    input.x(), input.y(), &min_x, &min_y);
  DCHECK(ret);
  ret = FPDF_PageToDevice(page, 0, 0, output_width, output_height, 0,
                          input.right(), input.bottom(), &max_x, &max_y);
  DCHECK(ret);

  if (max_x < min_x)
    std::swap(min_x, max_x);
  if (max_y < min_y)
    std::swap(min_y, max_y);

  pp::FloatRect output_rect(
      ConvertUnitDouble(min_x, kPointsPerInch, kPixelsPerInch),
      ConvertUnitDouble(min_y, kPointsPerInch, kPixelsPerInch),
      ConvertUnitDouble(max_x - min_x, kPointsPerInch, kPixelsPerInch),
      ConvertUnitDouble(max_y - min_y, kPointsPerInch, kPixelsPerInch));
  return output_rect;
}

pp::FloatRect GetFloatCharRectInPixels(FPDF_PAGE page,
                                       FPDF_TEXTPAGE text_page,
                                       int index) {
  double left;
  double right;
  double bottom;
  double top;
  FPDFText_GetCharBox(text_page, index, &left, &right, &bottom, &top);
  if (right < left)
    std::swap(left, right);
  if (bottom < top)
    std::swap(top, bottom);
  pp::FloatRect page_coords(left, top, right - left, bottom - top);
  return FloatPageRectToPixelRect(page, page_coords);
}

int GetFirstNonUnicodeWhiteSpaceCharIndex(FPDF_TEXTPAGE text_page,
                                          int start_char_index,
                                          int chars_count) {
  int i = start_char_index;
  while (i < chars_count &&
         base::IsUnicodeWhitespace(FPDFText_GetUnicode(text_page, i))) {
    i++;
  }
  return i;
}

PP_PrivateDirection GetDirectionFromAngle(double angle) {
  angle = fmod(angle + base::kPiDouble / 4.0, 2 * base::kPiDouble);
  if (angle >= 0 && angle <= base::kPiDouble / 2.0)
    return PP_PRIVATEDIRECTION_LTR;
  if (angle > base::kPiDouble / 2.0 && angle <= base::kPiDouble / 2.0)
    return PP_PRIVATEDIRECTION_TTB;
  if (angle > base::kPiDouble && angle <= 3 * base::kPiDouble / 2.0)
    return PP_PRIVATEDIRECTION_RTL;
  return PP_PRIVATEDIRECTION_BTT;
}

double GetDistanceBetweenPoints(const pp::FloatPoint& p1,
                                const pp::FloatPoint& p2) {
  pp::FloatPoint dist_vector = p1 - p2;
  return sqrt(pow(dist_vector.x(), 2) + pow(dist_vector.y(), 2));
}

void AddCharSizeToAverageCharSize(pp::FloatSize new_size,
                                  pp::FloatSize* avg_size,
                                  int* count) {
  // Some characters sometimes have a bogus empty bounding box. We don't want
  // them to impact the average.
  if (!new_size.IsEmpty()) {
    avg_size->set_width((avg_size->width() * *count + new_size.width()) /
                        (*count + 1));
    avg_size->set_height((avg_size->height() * *count + new_size.height()) /
                         (*count + 1));
    (*count)++;
  }
}

double GetRotatedCharWidth(double angle, const pp::FloatSize& size) {
  return abs(cos(angle) * size.width()) + abs(sin(angle) * size.height());
}

}  // namespace

PDFiumPage::LinkTarget::LinkTarget() : page(-1) {}

PDFiumPage::LinkTarget::LinkTarget(const LinkTarget& other) = default;

PDFiumPage::LinkTarget::~LinkTarget() = default;

PDFiumPage::PDFiumPage(PDFiumEngine* engine,
                       int i,
                       const pp::Rect& r,
                       bool available)
    : engine_(engine),
      index_(i),
      rect_(r),
      available_(available) {}

PDFiumPage::PDFiumPage(PDFiumPage&& that) = default;

PDFiumPage::~PDFiumPage() {
  DCHECK_EQ(0, preventing_unload_count_);
}

// static
void PDFiumPage::SetIsValidLinkFunctionForTesting(
    IsValidLinkFunction function) {
  g_is_valid_link_func_for_testing = function;
}

void PDFiumPage::Unload() {
  // Do not unload while in the middle of a load.
  if (preventing_unload_count_)
    return;

  text_page_.reset();

  if (page_) {
    if (engine_->form()) {
      FORM_OnBeforeClosePage(page(), engine_->form());
    }
    page_.reset();
  }
}

FPDF_PAGE PDFiumPage::GetPage() {
  ScopedUnsupportedFeature scoped_unsupported_feature(engine_);
  if (!available_)
    return nullptr;
  if (!page_) {
    ScopedUnloadPreventer scoped_unload_preventer(this);
    page_.reset(FPDF_LoadPage(engine_->doc(), index_));
    if (page_ && engine_->form()) {
      FORM_OnAfterLoadPage(page(), engine_->form());
    }
  }
  return page();
}

FPDF_TEXTPAGE PDFiumPage::GetTextPage() {
  if (!available_)
    return nullptr;
  if (!text_page_) {
    ScopedUnloadPreventer scoped_unload_preventer(this);
    text_page_.reset(FPDFText_LoadPage(GetPage()));
  }
  return text_page();
}

void PDFiumPage::GetTextRunInfo(int start_char_index,
                                uint32_t* out_len,
                                double* out_font_size,
                                pp::FloatRect* out_bounds) {
  FPDF_PAGE page = GetPage();
  FPDF_TEXTPAGE text_page = GetTextPage();
  int chars_count = FPDFText_CountChars(text_page);

  int char_index = GetFirstNonUnicodeWhiteSpaceCharIndex(
      text_page, start_char_index, chars_count);

  pp::FloatRect start_char_rect =
      GetFloatCharRectInPixels(page, text_page, char_index);
  int text_run_font_size = FPDFText_GetFontSize(text_page, char_index);

  // Heuristic: Initialize the average character size to one-third of the font
  // size to avoid having the first few characters misrepresent the average.
  // Without it, if a text run starts with a '.', its small bounding box could
  // lead to a break in the text run after only one space. Ex: ". Hello World"
  // would be split in two runs: "." and "Hello World".
  int font_size_minimum = FPDFText_GetFontSize(text_page, char_index) / 3.0;
  pp::FloatSize avg_char_size =
      pp::FloatSize(font_size_minimum, font_size_minimum);
  int non_whitespace_chars_count = 1;
  AddCharSizeToAverageCharSize(start_char_rect.Floatsize(), &avg_char_size,
                               &non_whitespace_chars_count);

  // Add first char to text run.
  pp::FloatRect text_run_bounds = start_char_rect;
  PP_PrivateDirection char_direction =
      GetDirectionFromAngle(FPDFText_GetCharAngle(text_page, char_index));
  if (char_index < chars_count)
    char_index++;

  pp::FloatRect prev_char_rect = start_char_rect;
  float estimated_font_size =
      std::max(start_char_rect.width(), start_char_rect.height());

  // Continue adding characters until heuristics indicate we should end the text
  // run.
  while (char_index < chars_count) {
    unsigned int character = FPDFText_GetUnicode(text_page, char_index);
    pp::FloatRect char_rect =
        GetFloatCharRectInPixels(page, text_page, char_index);

    if (!base::IsUnicodeWhitespace(character)) {
      // Heuristic: End the text run if different font size is encountered.
      int font_size = FPDFText_GetFontSize(text_page, char_index);
      if (font_size != text_run_font_size)
        break;

      // Heuristic: End text run if character isn't going in the same direction.
      if (char_direction !=
          GetDirectionFromAngle(FPDFText_GetCharAngle(text_page, char_index)))
        break;

      // Heuristic: End the text run if the center-point distance to the
      // previous character is less than 2.5x the average character size.
      AddCharSizeToAverageCharSize(char_rect.Floatsize(), &avg_char_size,
                                   &non_whitespace_chars_count);

      pp::FloatPoint current_prev_diff =
          char_rect.CenterPoint() - prev_char_rect.CenterPoint();
      double angle = atan2(current_prev_diff.y(), current_prev_diff.x());
      double avg_char_width = GetRotatedCharWidth(angle, avg_char_size);

      double distance =
          GetDistanceBetweenPoints(char_rect.CenterPoint(),
                                   prev_char_rect.CenterPoint()) -
          GetRotatedCharWidth(angle, char_rect.Floatsize()) / 2 -
          GetRotatedCharWidth(angle, prev_char_rect.Floatsize()) / 2;

      if (distance > 2.5 * avg_char_width)
        break;

      text_run_bounds = text_run_bounds.Union(char_rect);
      prev_char_rect = char_rect;
    }

    if (!char_rect.IsEmpty()) {
      // Update the estimated font size if needed.
      float char_largest_side = std::max(char_rect.height(), char_rect.width());
      estimated_font_size = std::max(char_largest_side, estimated_font_size);
    }

    char_index++;
  }

  // Some PDFs have missing or obviously bogus font sizes; substitute the
  // font size by the width or height (whichever's the largest) of the bigger
  // character in the current text run.
  if (text_run_font_size <= 1 || text_run_font_size < estimated_font_size / 2 ||
      text_run_font_size > estimated_font_size * 2) {
    text_run_font_size = estimated_font_size;
  }

  *out_len = char_index - start_char_index;
  *out_font_size = text_run_font_size;
  *out_bounds = text_run_bounds;
}

uint32_t PDFiumPage::GetCharUnicode(int char_index) {
  FPDF_TEXTPAGE text_page = GetTextPage();
  return FPDFText_GetUnicode(text_page, char_index);
}

pp::FloatRect PDFiumPage::GetCharBounds(int char_index) {
  FPDF_PAGE page = GetPage();
  FPDF_TEXTPAGE text_page = GetTextPage();
  return GetFloatCharRectInPixels(page, text_page, char_index);
}

PDFiumPage::Area PDFiumPage::GetCharIndex(const pp::Point& point,
                                          PageOrientation orientation,
                                          int* char_index,
                                          int* form_type,
                                          LinkTarget* target) {
  if (!available_)
    return NONSELECTABLE_AREA;
  pp::Point point2 = point - rect_.point();
  double new_x;
  double new_y;
  FPDF_BOOL ret = FPDF_DeviceToPage(
      GetPage(), 0, 0, rect_.width(), rect_.height(),
      ToPDFiumRotation(orientation), point2.x(), point2.y(), &new_x, &new_y);
  DCHECK(ret);

  // hit detection tolerance, in points.
  constexpr double kTolerance = 20.0;
  int rv = FPDFText_GetCharIndexAtPos(GetTextPage(), new_x, new_y, kTolerance,
                                      kTolerance);
  *char_index = rv;

  FPDF_LINK link = FPDFLink_GetLinkAtPoint(GetPage(), new_x, new_y);
  int control =
      FPDFPage_HasFormFieldAtPoint(engine_->form(), GetPage(), new_x, new_y);

  // If there is a control and link at the same point, figure out their z-order
  // to determine which is on top.
  if (link && control > FPDF_FORMFIELD_UNKNOWN) {
    int control_z_order = FPDFPage_FormFieldZOrderAtPoint(
        engine_->form(), GetPage(), new_x, new_y);
    int link_z_order = FPDFLink_GetLinkZOrderAtPoint(GetPage(), new_x, new_y);
    DCHECK_NE(control_z_order, link_z_order);
    if (control_z_order > link_z_order) {
      *form_type = control;
      return FormTypeToArea(*form_type);
    }

    // We don't handle all possible link types of the PDF. For example,
    // launch actions, cross-document links, etc.
    // In that case, GetLinkTarget() will return NONSELECTABLE_AREA
    // and we should proceed with area detection.
    Area area = GetLinkTarget(link, target);
    if (area != NONSELECTABLE_AREA)
      return area;
  } else if (link) {
    // We don't handle all possible link types of the PDF. For example,
    // launch actions, cross-document links, etc.
    // See identical block above.
    Area area = GetLinkTarget(link, target);
    if (area != NONSELECTABLE_AREA)
      return area;
  } else if (control > FPDF_FORMFIELD_UNKNOWN) {
    *form_type = control;
    return FormTypeToArea(*form_type);
  }

  if (rv < 0)
    return NONSELECTABLE_AREA;

  return GetLink(*char_index, target) != -1 ? WEBLINK_AREA : TEXT_AREA;
}

// static
PDFiumPage::Area PDFiumPage::FormTypeToArea(int form_type) {
  switch (form_type) {
    case FPDF_FORMFIELD_COMBOBOX:
    case FPDF_FORMFIELD_TEXTFIELD:
#if defined(PDF_ENABLE_XFA)
    // TODO(bug_353450): figure out selection and copying for XFA fields.
    case FPDF_FORMFIELD_XFA_COMBOBOX:
    case FPDF_FORMFIELD_XFA_TEXTFIELD:
#endif
      return FORM_TEXT_AREA;
    default:
      return NONSELECTABLE_AREA;
  }
}

base::char16 PDFiumPage::GetCharAtIndex(int index) {
  if (!available_)
    return L'\0';
  return static_cast<base::char16>(FPDFText_GetUnicode(GetTextPage(), index));
}

int PDFiumPage::GetCharCount() {
  if (!available_)
    return 0;
  return FPDFText_CountChars(GetTextPage());
}

PDFiumPage::Area PDFiumPage::GetLinkTarget(FPDF_LINK link, LinkTarget* target) {
  FPDF_DEST dest = FPDFLink_GetDest(engine_->doc(), link);
  if (dest)
    return GetDestinationTarget(dest, target);

  FPDF_ACTION action = FPDFLink_GetAction(link);
  if (!action)
    return NONSELECTABLE_AREA;

  switch (FPDFAction_GetType(action)) {
    case PDFACTION_GOTO: {
      FPDF_DEST dest = FPDFAction_GetDest(engine_->doc(), action);
      if (dest)
        return GetDestinationTarget(dest, target);
      // TODO(crbug.com/55776): We don't fully support all types of the
      // in-document links.
      return NONSELECTABLE_AREA;
    }
    case PDFACTION_URI:
      return GetURITarget(action, target);
    // TODO(crbug.com/767191): Support PDFACTION_LAUNCH.
    // TODO(crbug.com/142344): Support PDFACTION_REMOTEGOTO.
    case PDFACTION_LAUNCH:
    case PDFACTION_REMOTEGOTO:
    default:
      return NONSELECTABLE_AREA;
  }
}

PDFiumPage::Area PDFiumPage::GetDestinationTarget(FPDF_DEST destination,
                                                  LinkTarget* target) {
  if (!target)
    return NONSELECTABLE_AREA;

  int page_index = FPDFDest_GetDestPageIndex(engine_->doc(), destination);
  if (page_index < 0)
    return NONSELECTABLE_AREA;

  target->page = page_index;

  base::Optional<gfx::PointF> xy = GetPageXYTarget(destination);
  if (xy)
    target->y_in_pixels = TransformPageToScreenXY(xy.value()).y();

  return DOCLINK_AREA;
}

base::Optional<gfx::PointF> PDFiumPage::GetPageXYTarget(FPDF_DEST destination) {
  if (!available_)
    return {};

  FPDF_BOOL has_x_coord;
  FPDF_BOOL has_y_coord;
  FPDF_BOOL has_zoom;
  FS_FLOAT x;
  FS_FLOAT y;
  FS_FLOAT zoom;
  FPDF_BOOL success = FPDFDest_GetLocationInPage(
      destination, &has_x_coord, &has_y_coord, &has_zoom, &x, &y, &zoom);

  if (!success || !has_x_coord || !has_y_coord)
    return {};

  return {gfx::PointF(x, y)};
}

gfx::PointF PDFiumPage::TransformPageToScreenXY(const gfx::PointF& xy) {
  if (!available_)
    return gfx::PointF();

  pp::FloatRect page_rect(xy.x(), xy.y(), 0, 0);
  pp::FloatRect pixel_rect(FloatPageRectToPixelRect(GetPage(), page_rect));
  return gfx::PointF(pixel_rect.x(), pixel_rect.y());
}

PDFiumPage::Area PDFiumPage::GetURITarget(FPDF_ACTION uri_action,
                                          LinkTarget* target) const {
  if (target) {
    size_t buffer_size =
        FPDFAction_GetURIPath(engine_->doc(), uri_action, nullptr, 0);
    if (buffer_size > 0) {
      PDFiumAPIStringBufferAdapter<std::string> api_string_adapter(
          &target->url, buffer_size, true);
      void* data = api_string_adapter.GetData();
      size_t bytes_written =
          FPDFAction_GetURIPath(engine_->doc(), uri_action, data, buffer_size);
      api_string_adapter.Close(bytes_written);
    }
  }
  return WEBLINK_AREA;
}

int PDFiumPage::GetLink(int char_index, LinkTarget* target) {
  if (!available_)
    return -1;

  CalculateLinks();

  // Get the bounding box of the rect again, since it might have moved because
  // of the tolerance above.
  double left;
  double right;
  double bottom;
  double top;
  FPDFText_GetCharBox(GetTextPage(), char_index, &left, &right, &bottom, &top);

  pp::Point origin(PageToScreen(pp::Point(), 1.0, left, top, right, bottom,
                                PageOrientation::kOriginal)
                       .point());
  for (size_t i = 0; i < links_.size(); ++i) {
    for (const auto& rect : links_[i].bounding_rects) {
      if (rect.Contains(origin)) {
        if (target)
          target->url = links_[i].url;
        return i;
      }
    }
  }
  return -1;
}

void PDFiumPage::CalculateLinks() {
  if (calculated_links_)
    return;

  calculated_links_ = true;
  FPDF_PAGELINK links = FPDFLink_LoadWebLinks(GetTextPage());
  int count = FPDFLink_CountWebLinks(links);
  for (int i = 0; i < count; ++i) {
    base::string16 url;
    int url_length = FPDFLink_GetURL(links, i, nullptr, 0);
    if (url_length > 0) {
      PDFiumAPIStringBufferAdapter<base::string16> api_string_adapter(
          &url, url_length, true);
      unsigned short* data =
          reinterpret_cast<unsigned short*>(api_string_adapter.GetData());
      int actual_length = FPDFLink_GetURL(links, i, data, url_length);
      api_string_adapter.Close(actual_length);
    }
    Link link;
    link.url = base::UTF16ToUTF8(url);

    IsValidLinkFunction is_valid_link_func =
        g_is_valid_link_func_for_testing ? g_is_valid_link_func_for_testing
                                         : &IsValidLink;
    if (!is_valid_link_func(link.url))
      continue;

    // Make sure all the characters in the URL are valid per RFC 1738.
    // http://crbug.com/340326 has a sample bad PDF.
    // GURL does not work correctly, e.g. it just strips \t \r \n.
    bool is_invalid_url = false;
    for (size_t j = 0; j < link.url.length(); ++j) {
      // Control characters are not allowed.
      // 0x7F is also a control character.
      // 0x80 and above are not in US-ASCII.
      if (link.url[j] < ' ' || link.url[j] >= '\x7F') {
        is_invalid_url = true;
        break;
      }
    }
    if (is_invalid_url)
      continue;

    int rect_count = FPDFLink_CountRects(links, i);
    for (int j = 0; j < rect_count; ++j) {
      double left;
      double top;
      double right;
      double bottom;
      FPDFLink_GetRect(links, i, j, &left, &top, &right, &bottom);
      pp::Rect rect = PageToScreen(pp::Point(), 1.0, left, top, right, bottom,
                                   PageOrientation::kOriginal);
      if (rect.IsEmpty())
        continue;
      link.bounding_rects.push_back(rect);
    }
    FPDF_BOOL is_link_over_text = FPDFLink_GetTextRange(
        links, i, &link.start_char_index, &link.char_count);
    DCHECK(is_link_over_text);
    links_.push_back(link);
  }
  FPDFLink_CloseWebLinks(links);
}

bool PDFiumPage::GetUnderlyingTextRangeForRect(const pp::FloatRect& rect,
                                               int* start_index,
                                               uint32_t* char_len) {
  if (!available_)
    return false;

  FPDF_TEXTPAGE text_page = GetTextPage();
  const uint32_t char_count = FPDFText_CountChars(text_page);

  int start_char_index = -1;
  uint32_t cur_char_count = 0;

  // Iterate over page text to find such continuous characters whose mid-points
  // lie inside the rectangle.
  for (uint32_t i = 0; i < char_count; ++i) {
    double char_left;
    double char_right;
    double char_bottom;
    double char_top;
    if (!FPDFText_GetCharBox(text_page, i, &char_left, &char_right,
                             &char_bottom, &char_top)) {
      break;
    }

    float xmid = (char_left + char_right) / 2;
    float ymid = (char_top + char_bottom) / 2;
    if (rect.Contains(xmid, ymid)) {
      if (start_char_index == -1)
        start_char_index = i;
      ++cur_char_count;
    } else if (start_char_index != -1) {
      break;
    }
  }

  if (cur_char_count == 0)
    return false;

  *char_len = cur_char_count;
  *start_index = start_char_index;
  return true;
}

pp::Rect PDFiumPage::PageToScreen(const pp::Point& offset,
                                  double zoom,
                                  double left,
                                  double top,
                                  double right,
                                  double bottom,
                                  PageOrientation orientation) const {
  if (!available_)
    return pp::Rect();

  double start_x = (rect_.x() - offset.x()) * zoom;
  double start_y = (rect_.y() - offset.y()) * zoom;
  double size_x = rect_.width() * zoom;
  double size_y = rect_.height() * zoom;
  if (!base::IsValueInRangeForNumericType<int>(start_x) ||
      !base::IsValueInRangeForNumericType<int>(start_y) ||
      !base::IsValueInRangeForNumericType<int>(size_x) ||
      !base::IsValueInRangeForNumericType<int>(size_y)) {
    return pp::Rect();
  }

  int new_left;
  int new_top;
  int new_right;
  int new_bottom;
  FPDF_BOOL ret = FPDF_PageToDevice(
      page(), static_cast<int>(start_x), static_cast<int>(start_y),
      static_cast<int>(ceil(size_x)), static_cast<int>(ceil(size_y)),
      ToPDFiumRotation(orientation), left, top, &new_left, &new_top);
  DCHECK(ret);
  ret = FPDF_PageToDevice(
      page(), static_cast<int>(start_x), static_cast<int>(start_y),
      static_cast<int>(ceil(size_x)), static_cast<int>(ceil(size_y)),
      ToPDFiumRotation(orientation), right, bottom, &new_right, &new_bottom);
  DCHECK(ret);

  // If the PDF is rotated, the horizontal/vertical coordinates could be
  // flipped.  See
  // http://www.netl.doe.gov/publications/proceedings/03/ubc/presentations/Goeckner-pres.pdf
  if (new_right < new_left)
    std::swap(new_right, new_left);
  if (new_bottom < new_top)
    std::swap(new_bottom, new_top);

  base::CheckedNumeric<int32_t> new_size_x = new_right;
  new_size_x -= new_left;
  new_size_x += 1;
  base::CheckedNumeric<int32_t> new_size_y = new_bottom;
  new_size_y -= new_top;
  new_size_y += 1;
  if (!new_size_x.IsValid() || !new_size_y.IsValid())
    return pp::Rect();

  return pp::Rect(new_left, new_top, new_size_x.ValueOrDie(),
                  new_size_y.ValueOrDie());
}

const PDFEngine::PageFeatures* PDFiumPage::GetPageFeatures() {
  // If page_features_ is cached, return the cached features.
  if (page_features_.IsInitialized())
    return &page_features_;

  FPDF_PAGE page = GetPage();
  if (!page)
    return nullptr;

  // Initialize and cache page_features_.
  page_features_.index = index_;
  int annotation_count = FPDFPage_GetAnnotCount(page);
  for (int i = 0; i < annotation_count; ++i) {
    ScopedFPDFAnnotation annotation(FPDFPage_GetAnnot(page, i));
    FPDF_ANNOTATION_SUBTYPE subtype = FPDFAnnot_GetSubtype(annotation.get());
    page_features_.annotation_types.insert(subtype);
  }

  return &page_features_;
}

PDFiumPage::ScopedUnloadPreventer::ScopedUnloadPreventer(PDFiumPage* page)
    : page_(page) {
  page_->preventing_unload_count_++;
}

PDFiumPage::ScopedUnloadPreventer::~ScopedUnloadPreventer() {
  page_->preventing_unload_count_--;
}

PDFiumPage::Link::Link() = default;

PDFiumPage::Link::Link(const Link& that) = default;

PDFiumPage::Link::~Link() = default;

int ToPDFiumRotation(PageOrientation orientation) {
  // Could static_cast<int>(orientation), but using an exhaustive switch will
  // trigger an error if we ever change the definition of PageOrientation.
  switch (orientation) {
    case PageOrientation::kOriginal:
      return 0;
    case PageOrientation::kClockwise90:
      return 1;
    case PageOrientation::kClockwise180:
      return 2;
    case PageOrientation::kClockwise270:
      return 3;
  }
  NOTREACHED();
  return 0;
}

}  // namespace chrome_pdf
