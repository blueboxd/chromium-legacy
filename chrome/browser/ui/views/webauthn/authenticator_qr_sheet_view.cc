// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_qr_sheet_view.h"

#include "base/base64url.h"
#include "base/rand_util.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/ui/views/webauthn/authenticator_qr_code.h"
#include "ui/gfx/canvas.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

using QRCode = AuthenticatorQRCode;

namespace {

static constexpr int kDinoWidth = 20;
static constexpr int kDinoHeight = 22;
static constexpr int kDinoHeadHeight = 8;
static constexpr int kDinoWidthBytes = (kDinoWidth + 7) / 8;
static constexpr int kDinoBodyHeight = kDinoHeight - kDinoHeadHeight;

static const uint8_t kDinoHeadRight[kDinoWidthBytes * kDinoHeadHeight] = {
    // clang-format off
  0b00000000, 0b00011111, 0b11100000,
  0b00000000, 0b00111111, 0b11110000,
  0b00000000, 0b00110111, 0b11110000,
  0b00000000, 0b00111111, 0b11110000,
  0b00000000, 0b00111111, 0b11110000,
  0b00000000, 0b00111111, 0b11110000,
  0b00000000, 0b00111110, 0b00000000,
  0b00000000, 0b00111111, 0b11000000,
    // clang-format on
};

static const uint8_t kDinoHeadLeft[kDinoWidthBytes * kDinoHeadHeight] = {
    // clang-format off
  0b00000111, 0b11111000, 0b00000000,
  0b00001111, 0b11111100, 0b00000000,
  0b00001111, 0b11101100, 0b00000000,
  0b00001111, 0b11111100, 0b00000000,
  0b00001111, 0b11111100, 0b00000000,
  0b00001111, 0b11111100, 0b00000000,
  0b00000000, 0b01111100, 0b00000000,
  0b00000011, 0b11111100, 0b00000000,
    // clang-format on
};

static const uint8_t kDinoBody[kDinoWidthBytes * kDinoBodyHeight] = {
    // clang-format off
  0b10000000, 0b01111100, 0b00000000,
  0b10000001, 0b11111100, 0b00000000,
  0b11000011, 0b11111111, 0b00000000,
  0b11100111, 0b11111101, 0b00000000,
  0b11111111, 0b11111100, 0b00000000,
  0b11111111, 0b11111100, 0b00000000,
  0b01111111, 0b11111000, 0b00000000,
  0b00111111, 0b11111000, 0b00000000,
  0b00011111, 0b11110000, 0b00000000,
  0b00001111, 0b11100000, 0b00000000,
  0b00000111, 0b01100000, 0b00000000,
  0b00000110, 0b00100000, 0b00000000,
  0b00000100, 0b00100000, 0b00000000,
  0b00000110, 0b00110000, 0b00000000,
    // clang-format on
};

// QRView displays a QR code.
class QRView : public views::View {
 public:
  // kTilePixels is the height and width, in pixels, of a single tile from the
  // QR code.
  static constexpr int kTilePixels = 10;
  // kDinoTilePixels is the height and width, in pixels, of a single bit from
  // the dino image.
  static constexpr int kDinoTilePixels = 3;
  // kMid is the pixel offset from the top (or left) to the middle of the
  // displayed QR code.
  static constexpr int kMid = (kTilePixels * (2 + QRCode::kSize + 2)) / 2;
  // kDinoX is the x-coordinate of the dino image.
  static constexpr int kDinoX = kMid - (kDinoWidth * kDinoTilePixels) / 2;
  // kDinoY is the y-coordinate of the dino image.
  static constexpr int kDinoY = kMid - (kDinoHeight * kDinoTilePixels) / 2;

  explicit QRView(const uint8_t qr_data[QRCode::kInputBytes]) {
    static_assert(QRCode::kInputBytes == QRCode::kInputBytes,
                  "QR lengths mismatch");
    qr_tiles_ = qr_.Generate(qr_data);
  }
  ~QRView() override {}

  void RefreshQRCode(const uint8_t new_qr_data[QRCode::kInputBytes]) {
    state_++;
    qr_tiles_ = qr_.Generate(new_qr_data);
    SchedulePaint();
  }

  // View:
  gfx::Size CalculatePreferredSize() const override {
    // A two-tile border is required around the QR code.
    return gfx::Size((2 + QRCode::kSize + 2) * kTilePixels,
                     (2 + QRCode::kSize + 2) * kTilePixels);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    const SkColor off = SkColorSetARGB(0xff, 0xff, 0xff, 0xff);

    // kV is the intensity of the colors in the QR code.
    constexpr uint8_t kV = 0x70;
    SkColor on;
    switch (state_ % 6) {
      case 0:
        on = SkColorSetARGB(0xff, kV, 0, 0);
        break;
      case 1:
        on = SkColorSetARGB(0xff, 0, kV, 0);
        break;
      case 2:
        on = SkColorSetARGB(0xff, 0, 0, kV);
        break;
      case 3:
        on = SkColorSetARGB(0xff, kV, kV, 0);
        break;
      case 4:
        on = SkColorSetARGB(0xff, kV, 0, kV);
        break;
      case 5:
        on = SkColorSetARGB(0xff, 0, kV, kV);
        break;
    }

    // Draw the two-tile border around the edge.
    // Top.
    canvas->FillRect(
        gfx::Rect(0, 0, (2 + QRCode::kSize + 2) * kTilePixels, 2 * kTilePixels),
        off);
    // Bottom.
    canvas->FillRect(
        gfx::Rect(0, (2 + QRCode::kSize) * kTilePixels,
                  (2 + QRCode::kSize + 2) * kTilePixels, 2 * kTilePixels),
        off);
    // Left
    canvas->FillRect(gfx::Rect(0, 2 * kTilePixels, 2 * kTilePixels,
                               QRCode::kSize * kTilePixels),
                     off);
    // Right
    canvas->FillRect(
        gfx::Rect((2 + QRCode::kSize) * kTilePixels, 2 * kTilePixels,
                  2 * kTilePixels, QRCode::kSize * kTilePixels),
        off);

    // Paint the QR code.
    for (int y = 0; y < QRCode::kSize; y++) {
      for (int x = 0; x < QRCode::kSize; x++) {
        SkColor tile_color = (*qr_tiles_++) & 1 ? on : off;
        canvas->FillRect(gfx::Rect((x + 2) * kTilePixels, (y + 2) * kTilePixels,
                                   kTilePixels, kTilePixels),
                         tile_color);
      }
    }

    PaintDinoSegment(canvas, (state_ & 1) ? kDinoHeadLeft : kDinoHeadRight,
                     kDinoHeadHeight, 0);
    PaintDinoSegment(canvas, kDinoBody, kDinoHeight - kDinoHeadHeight,
                     kDinoHeadHeight);
  }

 private:
  void PaintDinoSegment(gfx::Canvas* canvas,
                        const uint8_t* data,
                        const int rows,
                        const int y_offset) {
    const SkColor color = SkColorSetARGB(0xff, 0x00, 0x00, 0x00);

    for (int y = 0; y < rows; y++) {
      uint8_t current_byte;
      unsigned bits = 0;

      for (int x = 0; x < kDinoWidth; x++) {
        if (bits == 0) {
          current_byte = *data++;
          bits = 8;
        }
        const bool is_set = (current_byte & 128) != 0;
        current_byte <<= 1;
        bits--;

        if (is_set) {
          canvas->FillRect(gfx::Rect(kDinoX + x * kDinoTilePixels,
                                     kDinoY + (y + y_offset) * kDinoTilePixels,
                                     kDinoTilePixels, kDinoTilePixels),
                           color);
        }
      }
    }
  }

  QRCode qr_;
  const uint8_t* qr_tiles_ = nullptr;
  unsigned state_ = 0;

  DISALLOW_COPY_AND_ASSIGN(QRView);
};

// RandomURL writes a caBLE key URL to |out_bytes|. This consists of the prefix
// "fido://c1/" followed by a base64url-encoded, 16-byte random key.
void RandomURL(uint8_t out_bytes[QRCode::kInputBytes]) {
  uint8_t rand_bytes[16];
  base::RandBytes(rand_bytes, sizeof(rand_bytes));
  std::string encoded;
  base::Base64UrlEncode(
      base::StringPiece(reinterpret_cast<const char*>(rand_bytes),
                        sizeof(rand_bytes)),
      base::Base64UrlEncodePolicy::OMIT_PADDING, &encoded);
  static_assert(QRCode::kInputBytes == 10 + 22, "QR input length mismatch");
  memcpy(out_bytes, "fido://c1/", 10);
  memcpy(&out_bytes[10], encoded.data(), 22);
}

}  // anonymous namespace

class AuthenticatorQRViewCentered : public views::View {
 public:
  explicit AuthenticatorQRViewCentered(
      const uint8_t qr_data[QRCode::kInputBytes]) {
    views::BoxLayout* layout =
        SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal));
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    qr_view_ = new QRView(qr_data);
    AddChildView(qr_view_);
  }

  void RefreshQRCode(const uint8_t new_qr_data[QRCode::kInputBytes]) {
    qr_view_->RefreshQRCode(new_qr_data);
  }

  QRView* qr_view_;
};

AuthenticatorQRSheetView::AuthenticatorQRSheetView(
    std::unique_ptr<AuthenticatorQRSheetModel> sheet_model)
    : AuthenticatorRequestSheetView(std::move(sheet_model)) {}

AuthenticatorQRSheetView::~AuthenticatorQRSheetView() = default;

void AuthenticatorQRSheetView::RefreshQRCode(
    const uint8_t new_qr_data[QRCode::kInputBytes]) {
  qr_view_->RefreshQRCode(new_qr_data);
}

std::unique_ptr<views::View>
AuthenticatorQRSheetView::BuildStepSpecificContent() {
  // TODO: data for the QR code should come from a caBLE discovery. Since that
  // isn't plumbed yet, we generate random data here.
  uint8_t qr_data[QRCode::kInputBytes];
  RandomURL(qr_data);

  auto qr_view = std::make_unique<AuthenticatorQRViewCentered>(qr_data);
  qr_view_ = qr_view.get();

  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(600), this,
               &AuthenticatorQRSheetView::Update);
  return qr_view;
}

void AuthenticatorQRSheetView::Update() {
  // TODO: fresh random values should come from the caBLE discovery. Until
  // that's plumbed in, generate them here.
  uint8_t qr_data[QRCode::kInputBytes];
  RandomURL(qr_data);
  qr_view_->RefreshQRCode(qr_data);
}
