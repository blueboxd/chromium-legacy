// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_mediator.h"

#import <CoreMedia/CoreMedia.h>
#import <Vision/Vision.h>

#include "base/logging.h"

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

- (void)processOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer {
  if (!self.textRecognitionRequest) {
    __weak __typeof(self) weakSelf = self;

    auto completionHandler = ^(VNRequest* request, NSError* error) {
      if (request.results.count != 0) {
        [weakSelf searchInRecognizedText:request.results];
      }
    };

    self.textRecognitionRequest = [[VNRecognizeTextRequest alloc]
        initWithCompletionHandler:completionHandler];

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
// TODO(crbug.com/984545): Add check for credit card number.
- (void)extractDataFromText:(NSString*)text {
  NSDateComponents* components = [self extractExpirationDateFromText:text];

  if (components) {
    self.expirationMonth = [@([components month]) stringValue];
    self.expirationYear = [@([components year]) stringValue];
  }
}

// Extracts expiration month and year from |text|.
- (NSDateComponents*)extractExpirationDateFromText:(NSString*)text {
  NSDateComponents* components;
  NSDateFormatter* formatter = [[NSDateFormatter init] alloc];
  [formatter setDateFormat:@"MM/yy"];
  NSDate* date = [formatter dateFromString:text];

  if (date) {
    NSCalendar* gregorian = [[NSCalendar alloc]
        initWithCalendarIdentifier:NSCalendarIdentifierGregorian];
    components = [gregorian components:NSCalendarUnitMonth | NSCalendarUnitYear
                              fromDate:date];
  }

  return components;
}

@end
