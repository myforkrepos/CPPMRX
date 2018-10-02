/*
   Implementation of CPPMRX class library for Arduino

   Copyright (C) 2017 Greg Tomasch and Simon D. Levy

   This file is part of CPPMRX.

   CPPMRX is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   CPPMRX is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   You should have received a copy of the GNU General Public License
   along with CPPMRX.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Arduino.h"
#include "CPPMRX.h"

static const uint8_t MAX_CHANS = 8;

static volatile uint16_t ppmTmp[MAX_CHANS];
static volatile uint32_t startPulse;
static volatile uint8_t  ppmCounter;
static volatile uint16_t ppmError;
static volatile uint16_t rcvr[MAX_CHANS];
static volatile bool got_new_frame;

static void isr(void)
{
    uint32_t stopPulse = micros();

    // clear channel interrupt flag (CHF)
    volatile uint32_t pulseWidth = stopPulse - startPulse;

    // Error sanity check
    if (pulseWidth < CPPMRX::MINPULSE || (pulseWidth > CPPMRX::MAXPULSE && pulseWidth < CPPMRX::SYNCPULSE)) {
        ppmError++;

        // set ppmCounter out of range so rest and (later on) whole frame is dropped
        ppmCounter = MAX_CHANS + 1;
    }
    if (pulseWidth >= CPPMRX::SYNCPULSE) {
        // Verify if this is the sync pulse
        if (ppmCounter <= MAX_CHANS) {
            // This indicates that we received a correct frame = push to the "main" PPM array
            // if we received an broken frame, it will get ignored here and later get over-written
            // by new data, that will also be checked for sanity.
            for (uint8_t i = 0; i < MAX_CHANS; i++) {
                rcvr[i] = ppmTmp[i];             
            }
        }

        // restart the channel counter
        ppmCounter = 0;

        got_new_frame = true;

    } else {  
        // extra channels will get ignored here
        if (ppmCounter < MAX_CHANS) {   
            // Store measured pulse length in us
            ppmTmp[ppmCounter] = pulseWidth;

            // Advance to next channel
            ppmCounter++;
        }
    }

    // Save time at pulse start
    startPulse = stopPulse;
}

CPPMRX::CPPMRX(uint8_t pin, uint8_t nchan)
{
    _pin = pin;
    _nchan = nchan;
}

void CPPMRX::begin()
{
    pinMode(_pin, INPUT);

    attachInterrupt(digitalPinToInterrupt(_pin), isr, RISING);

    for (uint8_t k=0; k<_nchan; ++k) {
        rcvr[k] = 1500;
        ppmTmp[k] = 1500;
    }

    ppmCounter = MAX_CHANS;
    ppmCounter = 0;
    ppmError = 0;
}

bool CPPMRX::gotNewFrame(void)
{
    bool retval = got_new_frame;
    if (got_new_frame) {
        got_new_frame = false;
    }
    return retval;
}

void CPPMRX::computeRC(uint16_t rcData[])
{
    static uint16_t rcData4Values[MAX_CHANS][4], rcDataMean[MAX_CHANS];
    static uint8_t rc4ValuesIndex = 0;
    uint32_t rawRC[MAX_CHANS];

    rc4ValuesIndex++;
    if (rc4ValuesIndex == 4) rc4ValuesIndex = 0;

    for (uint8_t k=0; k<_nchan; ++k) { 
        rawRC[k] = rcvr[k];
    }

    for (uint8_t chan=0; chan < _nchan; chan++) {
        rcData4Values[chan][rc4ValuesIndex] = rawRC[chan];
        rcDataMean[chan] = 0;
        for (uint8_t a=0; a<4; a++) rcDataMean[chan] += rcData4Values[chan][a];
        rcDataMean[chan]= (rcDataMean[chan] + 2) >> 2;
        if (rcDataMean[chan] < (uint16_t)rcData[chan] - 3) rcData[chan] = rcDataMean[chan] + 2;
        if (rcDataMean[chan] > (uint16_t)rcData[chan] + 3) rcData[chan] = rcDataMean[chan] - 2;
    }
}
