// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_mediator.h"

#import <CoreMedia/CoreMedia.h>
#import <Vision/Vision.h>

#include "base/logging.h"
#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_mediator_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CreditCardScannerMediator ()

// An image analysis request that finds and recognizes text in an image.
@property(nonatomic, strong) VNRecognizeTextRequest* textRecognitionRequest;

// The card number set after |textRecognitionRequest| from recognised text on
// the card.
@property(nonatomic, strong) NSString* cardNumber;

// The card expiration month set after |textRecognitionRequest| from recognised
// text on the card.
@property(nonatomic, strong) NSString* expirationMonth;

// The card expiration year set after |textRecognitionRequest| from recognised
// text on the card.
@property(nonatomic, strong) NSString* expirationYear;

@end

@implementation CreditCardScannerMediator

#pragma mark - CreditCardScannerImageDelegate

- (void)processOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
                         viewport:(CGRect)viewport {
  if (!self.textRecognitionRequest) {
    __weak __typeof(self) weakSelf = self;

    auto completionHandler = ^(VNRequest* request, NSError* error) {
      if (request.results.count != 0) {
        [weakSelf searchInRecognizedText:request.results];
      }
    };

    self.textRecognitionRequest = [[VNRecognizeTextRequest alloc]
        initWithCompletionHandler:completionHandler];

    // Sets the region of interest of the request to the scanner viewport to
    // focus on the scan area. This improves performance by ignoring irrelevant
    // background text.
    self.textRecognitionRequest.regionOfInterest = viewport;
    // Fast option doesn't recognise card number correctly.
    self.textRecognitionRequest.recognitionLevel =
        VNRequestTextRecognitionLevelAccurate;
    // For time performance as we scan for numbers and date only.
    self.textRecognitionRequest.usesLanguageCorrection = false;
  }

  CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
  DCHECK(pixelBuffer);

  NSMutableDictionary* options = [[NSMutableDictionary alloc] init];
  VNImageRequestHandler* handler =
      [[VNImageRequestHandler alloc] initWithCVPixelBuffer:pixelBuffer
                                                   options:options];

  NSError* requestError;
  [handler performRequests:@[ self.textRecognitionRequest ]
                     error:&requestError];

  // Error code 11 is unknown exception. It happens for some frames.
  DCHECK(!requestError || requestError.code == 11);
}

#pragma mark - Helper Methods

// Searches in |recognizedText| for credit card number and expiration date.
- (void)searchInRecognizedText:
    (NSArray<VNRecognizedTextObservation*>*)recognizedText {
  NSUInteger maximumCandidates = 1;

  for (VNRecognizedTextObservation* observation in recognizedText) {
    VNRecognizedText* candidate =
        [[observation topCandidates:maximumCandidates] firstObject];
    if (candidate == nil) {
      continue;
    }
    [self extractDataFromText:candidate.string];
  }
}

// Checks the type of |text| to assign it to appropriate property.
- (void)extractDataFromText:(NSString*)text {
  NSDateComponents* components = ios::ExtractExpirationDateFromText(text);

  if (components) {
    self.expirationMonth = [@([components month]) stringValue];
    self.expirationYear = [@([components year]) stringValue];
  }
  if (!self.cardNumber) {
    self.cardNumber = [self extractCreditCardNumber:text];
  }
}


// Extracts credit card number from |string|.
- (NSString*)extractCreditCardNumber:(NSString*)string {
  NSString* text = [[NSString alloc] initWithString:string];
  // Strip whitespaces and symbols.
  NSArray* ignoreSymbols = @[ @" ", @"/", @"-", @".", @":", @"\\" ];
  for (NSString* symbol in ignoreSymbols) {
    text = [text stringByReplacingOccurrencesOfString:symbol withString:@""];
  }

  // Matches strings which have 13-19 numbers between the start(^) and the
  // end($) of the line.
  NSString* pattern = @"^([0-9]{13,19})$";

  NSError* error;
  NSRegularExpression* regex = [[NSRegularExpression alloc]
      initWithPattern:pattern
              options:NSRegularExpressionAllowCommentsAndWhitespace
                error:&error];

  NSRange rangeOfText = NSMakeRange(0, [text length]);
  NSTextCheckingResult* match = [regex firstMatchInString:text
                                                  options:0
                                                    range:rangeOfText];
  if (!match) {
    return nil;
  }
  NSString* creditCardNumber = [text substringWithRange:match.range];
  return creditCardNumber;
}

@end
