// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

/**
 * An immutable class that mirrors org.chromium.payments.mojom.PaymentAddress.
 * https://w3c.github.io/payment-request/#paymentaddress-interface
 */
public class Address {
    public final String country;
    public final String[] addressLine;
    public final String region;
    public final String city;
    public final String dependentLocality;
    public final String postalCode;
    public final String sortingCode;
    public final String organization;
    public final String recipient;
    public final String phone;

    public Address() {
        country = "";
        addressLine = new String[0];
        region = "";
        city = "";
        dependentLocality = "";
        postalCode = "";
        sortingCode = "";
        organization = "";
        recipient = "";
        phone = "";
    }

    /**
     * @param country The country corresponding to the address.
     * @param addressLine The most specific part of the address. It can include, for example, a
     *         street name, a house number, apartment number, a rural delivery route, descriptive
     *         instructions, or a post office box number.
     * @param region The top level administrative subdivision of the country. For example, this can
     *         be a state, a province, an oblast, or a prefecture.
     * @param city The city/town portion of the address.
     * @param dependentLocalitly The dependent locality or sublocality within a city. For example,
     *         neighborhoods, boroughs, districts, or UK dependent localities.
     * @param postalCode The postal code or ZIP code, also known as PIN code in India.
     * @param sortingCode The sorting code as used in, for example, France.
     * @param organization The organization, firm, company, or institution at the address.
     * @param recipient The name of the recipient or contact person at the address.
     * @param phone The phone number of the recipient or contact person at the address.
     */
    public Address(String country, String[] addressLine, String region, String city,
            String dependentLocality, String postalCode, String sortingCode, String organization,
            String recipient, String phone) {
        this.country = country;
        this.addressLine = addressLine;
        this.region = region;
        this.city = city;
        this.dependentLocality = dependentLocality;
        this.postalCode = postalCode;
        this.sortingCode = sortingCode;
        this.organization = organization;
        this.recipient = recipient;
        this.phone = phone;
    }
}
