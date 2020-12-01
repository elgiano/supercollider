/*
    SuperCollider real time audio synthesis system
    Copyright (c) 2002 James McCartney. All rights reserved.
    http://www.audiosynth.com

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
 *  BELA I/O UGens created by nescivi, (c) 2016
 *  https://www.nescivi.eu
 */

// #include <SC_Lock.h>

#include <atomic>

#include "Bela.h"
// These functions are provided by xenomai
int rt_printf(const char* format, ...);
int rt_fprintf(FILE* stream, const char* format, ...);

#include "SC_PlugIn.h"

class AccessBuffer {
public:
    AccessBuffer(float* buffer, unsigned int count) : buffer(buffer), count(count), last(buffer[count - 1]) {}
protected:
    const float& at(unsigned int n) const {
        if(n < count)
            return buffer[n];
        else
            return last;
    }
    float& at(unsigned int n) {
        if(n < count)
            return buffer[n];
        else
            return last;
    }
    float* buffer;
    float last;
    const unsigned int count;
};

// a buffer view which on which you can call [] for arbitrarily large numbers, but
// if it exceeds count, you will get back the value it had at [count-1] at
// initialisation
// additionally, upon destruction the [count-1] element wil be replaced with
// the cached value, which may have been overridden when assigning the returned
// value.
// this is useful when dealing with Sc buffers regardless of them being audio or control rate.
// it is also safe against overlapping buffers, as long as:
// - one object only writes to the buffer one or more others only reads from it
// - you write to them incrementally (from 0 to count - 1)
// - for each element, you write after reading (note: this provides safety in
// that you can keep calling [n] with n >= count and you will still access the
// valid cached value and not whathever the writer may have written to that
// since)
class AccessBufferWriter : public AccessBuffer {
public:
    AccessBufferWriter(float* buffer, unsigned int count) : AccessBuffer(buffer, count) {};
    ~AccessBufferWriter() {
        // put the last value written back in the buffer
        buffer[count - 1] = last;
    }
    float& operator[] (unsigned int n) {
        return at(n);
    }
};
class AccessBufferReader : public AccessBuffer {
public:
    AccessBufferReader(float* buffer, unsigned int count) : AccessBuffer(buffer, count) {};
    const float& operator[] (unsigned int n) const {
        return AccessBuffer::at(n);
    }
};

static InterfaceTable* ft;

static inline void BelaUgen_init_output(Unit* unit) { (unit->mCalcFunc)(unit, 1); }

static inline void BelaUgen_disable(Unit* unit) {
    SETCALC(ClearUnitOutputs);
    BelaUgen_init_output(unit);
}

struct MultiplexAnalogIn : public Unit {
};


struct AnalogIn : public Unit {
    int mAnalogPin;
};

struct AnalogOut : public Unit {
    int mAnalogPin;
};

// static digital pin, static function (in)
struct DigitalIn : public Unit {
    int mDigitalPin;
};

// static digital pin, static function (out) - uses DigitalWrite and a check whether value changed
struct DigitalOut : public Unit {
    int mDigitalPin;
    int mLastOut;
};

// static digital pin, static function (out) - uses DigitalWriteOnce
struct DigitalOutA : public Unit {
    int mDigitalPin;
    int mLastOut;
};

// flexible digital pin, flexible function (in or out)
struct DigitalIO : public Unit {
    int mDigitalPin;
    int mLastDigitalIn;
    int mLastDigitalOut;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

void MultiplexAnalogIn_next_aaa(MultiplexAnalogIn* unit, int inNumSamples) {
    World* world = unit->mWorld;
    BelaContext* context = world->mBelaContext;

    float* fin = IN(0); // analog in pin, can be modulated
    float* fmux = IN(1); // mux channel, can be modulated
    float* out = ZOUT(0);
    int analogPin = 0;
    int muxChannel = 0;
    float analogValue = 0;

    // context->audioFrames should be equal to inNumSamples
    //   for(unsigned int n = 0; n < context->audioFrames; n++) {
    for (unsigned int n = 0; n < inNumSamples; n++) {
        analogPin = (int)fin[n];
        muxChannel = (int)fmux[n];
        if ((analogPin < 0) || (analogPin >= context->analogInChannels) || (muxChannel < 0)
            || (muxChannel > context->multiplexerChannels)) {
            rt_fprintf(stderr, "MultiplexAnalogIn warning: analog pin must be between %i and %i, it is %i \n", 0,
                      context->analogInChannels, analogPin);
            rt_fprintf(stderr, "MultiplexAnalogIn warning: muxChannel must be between %i and %i, it is %i \n", 0,
                      context->multiplexerChannels, muxChannel);
        } else {
            analogValue = multiplexerAnalogRead(
                context, analogPin, muxChannel); // is there something like NI? analogReadNI(context, 0, analogPin);
            //         if(analogPin == 0)
            //         {
            //             static int count = 0;
            //             count++;
            //             if(count % 20000 == 0)
            //                 rt_printf("MultiPlexed AnalogValue = %.3f\n", analogValue);
            //         }
        }
        *++out = analogValue;
    }
}

void MultiplexAnalogIn_next_aak(MultiplexAnalogIn* unit, int inNumSamples) {
    World* world = unit->mWorld;
    BelaContext* context = world->mBelaContext;

    float* fin = IN(0); // analog in pin, can be modulated
    int muxChannel = (float)IN0(1);
    float* out = ZOUT(0);
    int analogPin = 0;
    float analogValue = 0;

    // context->audioFrames should be equal to inNumSamples
    //   for(unsigned int n = 0; n < context->audioFrames; n++) {
    for (unsigned int n = 0; n < inNumSamples; n++) {
        analogPin = (int)fin[n];
        if ((analogPin < 0) || (analogPin >= context->analogInChannels) || (muxChannel < 0)
            || (muxChannel > context->multiplexerChannels)) {
            rt_fprintf(stderr, "MultiplexAnalogIn warning: analog pin must be between %i and %i, it is %i \n", 0,
                      context->analogInChannels, analogPin);
            rt_fprintf(stderr, "MultiplexAnalogIn warning: muxChannel must be between %i and %i, it is %i \n", 0,
                      context->multiplexerChannels, muxChannel);
        } else {
            analogValue = multiplexerAnalogRead(
                context, analogPin, muxChannel); // is there something like NI? analogReadNI(context, 0, analogPin);
            //         if(analogPin == 0)
            //         {
            //             static int count = 0;
            //             count++;
            //             if(count % 20000 == 0)
            //                 rt_printf("MultiPlexed AnalogValue = %.3f\n", analogValue);
            //         }
        }
        *++out = analogValue;
    }
}

void MultiplexAnalogIn_next_aka(MultiplexAnalogIn* unit, int inNumSamples) {
    World* world = unit->mWorld;
    BelaContext* context = world->mBelaContext;

    int analogPin = (float)IN0(0);
    float* fmux = IN(1); // mux channel, can be modulated
    float* out = ZOUT(0);
    int muxChannel = 0;
    float analogValue = 0;

    // context->audioFrames should be equal to inNumSamples
    //   for(unsigned int n = 0; n < context->audioFrames; n++) {
    for (unsigned int n = 0; n < inNumSamples; n++) {
        muxChannel = (int)fmux[n];
        if ((analogPin < 0) || (analogPin >= context->analogInChannels) || (muxChannel < 0)
            || (muxChannel > context->multiplexerChannels)) {
            rt_fprintf(stderr, "MultiplexAnalogIn warning: analog pin must be between %i and %i, it is %i \n", 0,
                      context->analogInChannels, analogPin);
            rt_fprintf(stderr, "MultiplexAnalogIn warning: muxChannel must be between %i and %i, it is %i \n", 0,
                      context->multiplexerChannels, muxChannel);
        } else {
            analogValue = multiplexerAnalogRead(
                context, analogPin, muxChannel); // is there something like NI? analogReadNI(context, 0, analogPin);
            //         if(analogPin == 0)
            //         {
            //             static int count = 0;
            //             count++;
            //             if(count % 20000 == 0)
            //                 rt_printf("MultiPlexed AnalogValue = %.3f\n", analogValue);
            //         }
        }
        *++out = analogValue;
    }
}

void MultiplexAnalogIn_next_akk(MultiplexAnalogIn* unit, int inNumSamples) {
    World* world = unit->mWorld;
    BelaContext* context = world->mBelaContext;

    int analogPin = (float)IN0(0);
    int muxChannel = (float)IN0(1);
    float* out = ZOUT(0);
    float analogValue = 0;

    if ((analogPin < 0) || (analogPin >= context->analogInChannels) || (muxChannel < 0)
        || (muxChannel > context->multiplexerChannels)) {
        rt_fprintf(stderr, "MultiplexAnalogIn warning: analog pin must be between %i and %i, it is %i \n", 0,
                  context->analogInChannels, analogPin);
        rt_fprintf(stderr, "MultiplexAnalogIn warning: muxChannel must be between %i and %i, it is %i \n", 0,
                  context->multiplexerChannels, muxChannel);
        for (unsigned int n = 0; n < inNumSamples; n++) {
            *++out = 0;
        }
    } else {
        for (unsigned int n = 0; n < inNumSamples; n++) {
            analogValue = multiplexerAnalogRead(
                context, analogPin, muxChannel); // is there something like NI? analogReadNI(context, 0, analogPin);
            //             if(analogPin == 0)
            //             {
            //                 static int count = 0;
            //                 count++;
            //                 if(count % 20000 == 0)
            //                     rt_printf("MultiPlexed AnalogValue = %.3f\n", analogValue);
            //             }
            *++out = analogValue;
        }
    }
}

void MultiplexAnalogIn_next_kkk(MultiplexAnalogIn* unit, int inNumSamples) {
    World* world = unit->mWorld;
    BelaContext* context = world->mBelaContext;

    int analogPin = (float)IN0(0);
    int muxChannel = (float)IN0(1);

    if ((analogPin < 0) || (analogPin >= context->analogInChannels)) {
        rt_fprintf(stderr, "MultiplexAnalogIn warning: analog pin must be between %i and %i, it is %i \n", 0,
                  context->analogInChannels, analogPin);
        ZOUT0(0) = 0.0;
    } else if ((muxChannel < 0) || (muxChannel > context->multiplexerChannels)) {
        rt_fprintf(stderr, "MultiplexAnalogIn warning: muxChannel must be between %i and %i, it is %i \n", 0,
                  context->multiplexerChannels, muxChannel);
        ZOUT0(0) = 0.0;
    } else {
        ZOUT0(0) = multiplexerAnalogRead(
            context, analogPin, muxChannel); // is there something like NI? analogReadNI(context, 0, analogPin);
    }
}

void MultiplexAnalogIn_Ctor(MultiplexAnalogIn* unit) {
    BelaContext* context = unit->mWorld->mBelaContext;

    if (!context->multiplexerChannels) {
        BelaUgen_disable(unit);
        rt_fprintf(stderr, "MultiplexAnalogIn Error: the UGen needs BELA Multiplexer Capelet enabled\n");
        return;
    }

    // set calculation method
    if (unit->mCalcRate == calc_FullRate) {
        if (INRATE(0) == calc_FullRate) {
            if (INRATE(1) == calc_FullRate) {
                SETCALC(MultiplexAnalogIn_next_aaa);
            } else {
                //                 rt_printf("AnalogIn: aa\n");
                SETCALC(MultiplexAnalogIn_next_aak);
            }
        } else {
            if (INRATE(1) == calc_FullRate) {
                SETCALC(MultiplexAnalogIn_next_aka);
            } else {
                //                 rt_printf("AnalogIn: ak\n");
                SETCALC(MultiplexAnalogIn_next_akk);
            }
        }
    } else {
        if ((INRATE(0) == calc_FullRate) || (INRATE(1) == calc_FullRate)) {
            rt_fprintf(stderr, "MultiplexAnalogIn warning: output rate is control rate, so cannot change analog pin or "
                      "multiplex channel at audio rate\n");
        }
        //             rt_printf("AnalogIn: kk\n");
        SETCALC(MultiplexAnalogIn_next_kkk);
    }
    BelaUgen_init_output(unit);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

static bool updatePin(unsigned int numChannels, unsigned int newPin, int* oldPin, const char* label)
{
    bool isValid = newPin < numChannels;
    if (newPin != *oldPin) {
        *oldPin = newPin;
        if (!isValid) {
            rt_fprintf(stderr, "%s warning: pin must be 0 <= pin <= %i, it is %i \n",
                label, numChannels - 1, newPin);
        }
    }
    return isValid;
}

// returns false if pin is out of range, so that _next functions should avoid using it
bool AnalogIn_updatePin(AnalogIn* unit, int newPin) {
BelaContext* context = unit->mWorld->mBelaContext;
    return updatePin(context->analogInChannels, newPin, &unit->mAnalogPin, "AnalogIn");
}

void AnalogIn_next_aa(AnalogIn* unit, int inNumSamples) {
    World* world = unit->mWorld;
    BelaContext* context = world->mBelaContext;

    float* fin = IN(0); // analog in pin, can be modulated
    float* out = ZOUT(0);
    float analogValue = 0;

    for (unsigned int n = 0; n < inNumSamples; n++) {
        int analogPin = (int)fin[n];
        if (AnalogIn_updatePin(unit, analogPin)) {
            analogValue = analogReadNI(context, n, analogPin);
        }
        *++out = analogValue;
    }
}

void AnalogIn_next_ak(AnalogIn* unit, int inNumSamples) {
    World* world = unit->mWorld;
    BelaContext* context = world->mBelaContext;

    int analogPin = (int)IN0(0);
    float* out = ZOUT(0);
    float analogValue = 0;

    if (AnalogIn_updatePin(unit, analogPin)) {
        for (unsigned int n = 0; n < inNumSamples; n++) {
            analogValue = analogReadNI(context, n, analogPin);
            *++out = analogValue;
        }
    } else {
        for (unsigned int n = 0; n < inNumSamples; n++) {
            *++out = 0.0;
        }
    }
}


void AnalogIn_next_kk(AnalogIn* unit, int inNumSamples) {
    World* world = unit->mWorld;
    BelaContext* context = world->mBelaContext;

    int analogPin = (int)IN0(0);

    if (AnalogIn_updatePin(unit, analogPin)) {
        ZOUT0(0) = analogReadNI(context, 0, analogPin);
    } else {
        ZOUT0(0) = 0.0;
    }
}

void AnalogIn_Ctor(AnalogIn* unit) {
    BelaContext* context = unit->mWorld->mBelaContext;

    if (!context->analogInChannels) {
        BelaUgen_disable(unit);
        rt_fprintf(stderr, "AnalogIn Error: the UGen needs BELA analog inputs enabled\n");
        return;
    }

    unit->mAnalogPin = -1;

    // set calculation method
    if (unit->mCalcRate == calc_FullRate) {
        if (INRATE(0) == calc_FullRate) {
            //                 rt_printf("AnalogIn: aa\n");
            SETCALC(AnalogIn_next_aa);
        } else {
            //                 rt_printf("AnalogIn: ak\n");
            SETCALC(AnalogIn_next_ak);
        }
    } else {
        if (INRATE(0) == calc_FullRate) {
            rt_fprintf(stderr, "AnalogIn warning: output rate is control rate, so cannot change analog pin at audio rate\n");
        }
        //             rt_printf("AnalogIn: kk\n");
        SETCALC(AnalogIn_next_kk);
    }
    BelaUgen_init_output(unit);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

// returns false if pin is out of range, so that _next functions should avoid using it
bool AnalogOut_updatePin(AnalogOut* unit, int newPin) {
    BelaContext* context = unit->mWorld->mBelaContext;
    return updatePin(context->analogOutChannels, newPin, &unit->mAnalogPin, "AnalogOut");
}

void AnalogOut_next_aaa(AnalogOut* unit, int inNumSamples) {
    World* world = unit->mWorld;
    BelaContext* context = world->mBelaContext;

    float* fin = IN(0); // analog in pin, can be modulated
    float* in = IN(1);

    int analogPin = 0;
    float newinput = 0;
    for (unsigned int n = 0; n < inNumSamples; n++) {
        // read input
        analogPin = (int)fin[n];
        if (AnalogOut_updatePin(unit, analogPin)) {
            newinput = in[n]; // read next input sample
            analogWriteOnceNI(context, n, unit->mAnalogPin, newinput);
        }
    }
}

void AnalogOut_next_aka(AnalogOut* unit, int inNumSamples) {
    World* world = unit->mWorld;
    BelaContext* context = world->mBelaContext;

    int analogPin = (int)IN0(0); // analog in pin, can be modulated
    float* in = IN(1);

    float newinput = 0;
    if (AnalogOut_updatePin(unit, analogPin)) {
        for (unsigned int n = 0; n < inNumSamples; n++) {
            newinput = in[n]; // read next input sample
            analogWriteOnceNI(context, n, unit->mAnalogPin, newinput);
        }
    }
}

void AnalogOut_next_aak(AnalogOut* unit, int inNumSamples) {
    World* world = unit->mWorld;
    BelaContext* context = world->mBelaContext;

    float* fin = IN(0); // analog in pin, can be modulated
    float in = IN0(1);

    int analogPin = 0;
    for (unsigned int n = 0; n < inNumSamples; n++) {
        // read input
        analogPin = (int)fin[n];
        if (AnalogOut_updatePin(unit, analogPin)) {
            analogWriteOnceNI(context, n, unit->mAnalogPin, in);
        }
    }
}

void AnalogOut_next_kk(AnalogOut* unit, int inNumSamples) {
    World* world = unit->mWorld;
    BelaContext* context = world->mBelaContext;

    int analogPin = (int)IN0(0); // analog in pin, can be modulated
    float in = IN0(1);

    if (AnalogOut_updatePin(unit, analogPin)) {
        analogWriteNI(context, 0, unit->mAnalogPin, in);
    }
}

void AnalogOut_Ctor(AnalogOut* unit) {
    BelaContext* context = unit->mWorld->mBelaContext;

    if (!context->analogOutChannels) {
        BelaUgen_disable(unit);
        rt_fprintf(stderr, "AnalogOut Error: the UGen needs BELA analog outputs enabled\n");
        return;
    }

    unit->mAnalogPin = -1;

    if (unit->mCalcRate == calc_FullRate) { // ugen running at audio rate;
        if (INRATE(0) == calc_FullRate) { // pin changed at audio rate
            if (INRATE(1) == calc_FullRate) { // output changed at audio rate
                SETCALC(AnalogOut_next_aaa);
                //                     rt_printf("AnalogOut: aaa\n");
            } else {
                SETCALC(AnalogOut_next_aak);
                //                     rt_printf("AnalogOut: aak\n");
            }
        } else { // pin changed at control rate
            if (INRATE(1) == calc_FullRate) { // output changed at audio rate
                SETCALC(AnalogOut_next_aka);
                //                     rt_printf("AnalogOut: aka\n");
            } else { // analog output only changes at control rate anyways
                rt_fprintf(stderr, "AnalogOut warning: inputs are control rate, so AnalogOut is also running at control rate\n");
                //                     rt_printf("AnalogOut: kk\n");
                SETCALC(AnalogOut_next_kk);
            }
        }
    } else { // ugen at control rate
        if ((INRATE(0) == calc_FullRate) || (INRATE(1) == calc_FullRate)) {
            rt_fprintf(stderr, "AnalogOut warning: output rate is control rate, so cannot change inputs at audio rate\n");
        }
        //             rt_printf("AnalogOut: kk\n");
        SETCALC(AnalogOut_next_kk);
    }
    BelaUgen_init_output(unit);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void DigitalIn_next_a(DigitalIn* unit, int inNumSamples) {
    World* world = unit->mWorld;
    BelaContext* context = world->mBelaContext;

    int pinid = unit->mDigitalPin;
    int digitalValue;
    float* out = ZOUT(0);

    for (unsigned int n = 0; n < inNumSamples; n++) {
        digitalValue = digitalRead(context, n, pinid);
        *++out = (float)digitalValue;
    }
}

void DigitalIn_next_k(DigitalIn* unit, int inNumSamples) {
    World* world = unit->mWorld;
    BelaContext* context = world->mBelaContext;

    int pinid = unit->mDigitalPin;
    int digitalValue = digitalRead(context, 0, pinid);
    ZOUT0(0) = (float)digitalValue;
}

void DigitalIn_Ctor(DigitalIn* unit) {
    BelaContext* context = unit->mWorld->mBelaContext;

    float fDigitalIn = ZIN0(0); // digital in pin -- cannot change after construction
    unit->mDigitalPin = (int)fDigitalIn;
    if ((unit->mDigitalPin < 0) || (unit->mDigitalPin >= context->digitalChannels)) {
        rt_fprintf(stderr, "DigitalIn error: digital pin must be between %i and %i, it is %i\n", 0, context->digitalChannels,
                  unit->mDigitalPin);
        BelaUgen_disable(unit);
        return;
    }
    pinMode(context, 0, unit->mDigitalPin, INPUT);
    // set calculation method
    if (unit->mCalcRate == calc_FullRate) { // ugen running at audio rate;
        SETCALC(DigitalIn_next_a);
        //                 rt_printf("DigitalIn: a\n");
    } else {
        SETCALC(DigitalIn_next_k);
        //                 rt_printf("DigitalIn: k\n");
    }
    BelaUgen_init_output(unit);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void DigitalOut_next_a_once(DigitalOut* unit, int inNumSamples) {
    World* world = unit->mWorld;
    BelaContext* context = world->mBelaContext;

    int pinid = unit->mDigitalPin;
    float* in = IN(1);

    for (unsigned int n = 0; n < inNumSamples; n++) {
        float newinput = in[n];
        if (newinput > 0.5) {
            digitalWriteOnce(context, n, pinid, 1);
        } else {
            digitalWriteOnce(context, n, pinid, 0);
        }
    }
}

void DigitalOut_next_a(DigitalOut* unit, int inNumSamples) {
    World* world = unit->mWorld;
    BelaContext* context = world->mBelaContext;

    int pinid = unit->mDigitalPin;
    float* in = IN(1);

    float newinput = 0;
    int lastOut = unit->mLastOut;

    for (unsigned int n = 0; n < inNumSamples; n++) {
        // read input
        newinput = in[n];
        if (newinput > 0.5) {
            if (lastOut == 0) {
                lastOut = 1;
                digitalWrite(context, n, pinid, 1);
            }
        } else if (lastOut == 1) {
            lastOut = 0;
            digitalWrite(context, n, pinid, 0);
        }
    }
    unit->mLastOut = lastOut;
}

void DigitalOut_next_k(DigitalOut* unit, int inNumSamples) {
    World* world = unit->mWorld;
    BelaContext* context = world->mBelaContext;

    int pinid = unit->mDigitalPin;
    float in = IN0(1);

    int lastOut = unit->mLastOut;
    if (in > 0.5) {
        if (lastOut == 0) {
            lastOut = 1;
            digitalWrite(context, 0, pinid, 1);
        }
    } else if (lastOut == 1) {
        lastOut = 0;
        digitalWrite(context, 0, pinid, 0);
    }
    unit->mLastOut = lastOut;
}

void DigitalOut_Ctor(DigitalOut* unit) {
    BelaContext* context = unit->mWorld->mBelaContext;

    float fDigital = ZIN0(0); // digital in pin -- cannot change after construction
    // method of writing; 1 = writeOnce; 0 = write on change -- cannot change
    // after construction
    int writeMode = (int)ZIN0(2);
    unit->mDigitalPin = (int)fDigital;
    unit->mLastOut = 0;

    if ((unit->mDigitalPin < 0) || (unit->mDigitalPin >= context->digitalChannels)) {
        rt_fprintf(stderr, "DigitalOut error: digital pin must be between %i and %i, it is %i \n", 0, context->digitalChannels,
                  unit->mDigitalPin);
        BelaUgen_disable(unit);
    }
    // initialize first buffer
    pinMode(context, 0, unit->mDigitalPin, OUTPUT);
    digitalWrite(context, 0, unit->mDigitalPin, unit->mLastOut);

    if (unit->mCalcRate == calc_FullRate) { // ugen running at audio rate;
        if (INRATE(1) == calc_FullRate) { // output changed at audio rate
            if (writeMode) {
                //                     rt_printf("DigitalOut: a once\n");
                SETCALC(DigitalOut_next_a_once);
            } else {
                //                     rt_printf("DigitalOut: a\n");
                SETCALC(DigitalOut_next_a);
            }
        } else { // not much reason to actually do audiorate output
            rt_fprintf(stderr, "DigitalOut warning: inputs are control rate, so DigitalOut will run at control rate\n");
            //                 rt_printf("DigitalOut: k\n");
            SETCALC(DigitalOut_next_k);
        }
    } else { // ugen at control rate
        if (INRATE(1) == calc_FullRate) {
            rt_fprintf(stderr, "DigitalOut warning: UGen rate is control rate, so cannot change inputs at audio rate\n");
        }
        //             rt_printf("DigitalOut: k\n");
        SETCALC(DigitalOut_next_k);
    }
    BelaUgen_init_output(unit);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

// returns false if pin is out of range, so that _next functions should avoid using it
bool DigitalIO_updatePin(DigitalIO* unit, int newPin) {
    BelaContext* context = unit->mWorld->mBelaContext;
    return updatePin(context->digitalChannels, newPin, &unit->mDigitalPin, "DigitalIO");
}

void DigitalIO_next_aaaa_once(DigitalIO* unit, int inNumSamples) {
    World* world = unit->mWorld;
    BelaContext* context = world->mBelaContext;

    float* pinid = IN(0);
    float* in = IN(1); // input value
    float* iomode = IN(2); // IO mode : < 0.5 = input, else output
    float* out = ZOUT(0); // output value = last output value

    int newDigInInt = unit->mLastDigitalIn;
    float newDigIn = (float)newDigInInt;
    int newDigOut = unit->mLastDigitalOut;

    for (unsigned int n = 0; n < inNumSamples; n++) {
        // read input
        if (DigitalIO_updatePin(unit, pinid[n])) {
            float newmode = iomode[n];
            if (newmode < 0.5) {
                pinModeOnce(context, n, unit->mDigitalPin, INPUT);
                newDigInInt = digitalRead(context, n, unit->mDigitalPin);
            } else {
                newDigOut = (int)in[n];
                pinModeOnce(context, n, unit->mDigitalPin, OUTPUT);
                digitalWriteOnce(context, n, unit->mDigitalPin, newDigOut);
            }
        }
        // always write to the output of the UGen
        *++out = (float)newDigInInt;
    }
    unit->mLastDigitalIn = newDigInInt;
    unit->mLastDigitalOut = newDigOut;
}


void DigitalIO_next_aaak_once(DigitalIO* unit, int inNumSamples) {
    // pinMode at control rate
    World* world = unit->mWorld;
    BelaContext* context = world->mBelaContext;

    float* pinid = IN(0);
    float* in = IN(1); // input value
    float iomode = IN0(2); // IO mode : < 0.5 = input, else output
    float* out = ZOUT(0); // output value = last output value

    int newDigInInt = unit->mLastDigitalIn;
    float newDigIn = (float)newDigInInt;

    int newDigOut = unit->mLastDigitalOut;

    if (iomode < 0.5) {
        for (unsigned int n = 0; n < inNumSamples; n++) {
            if (DigitalIO_updatePin(unit, pinid[n])) {
                pinModeOnce(context, n, unit->mDigitalPin, INPUT);
                newDigInInt = digitalRead(context, n, unit->mDigitalPin);
            }
            // always write to the output of the UGen
            *++out = (float)newDigInInt;
        }
    } else {
        for (unsigned int n = 0; n < inNumSamples; n++) {
            if (DigitalIO_updatePin(unit, pinid[n])) {
                pinModeOnce(context, n, unit->mDigitalPin, OUTPUT);
                newDigOut = (int)in[n];
                digitalWriteOnce(context, n, unit->mDigitalPin, newDigOut);
            }
            *++out = (float)newDigInInt;
        }
    }
    unit->mLastDigitalIn = newDigInInt;
    unit->mLastDigitalOut = newDigOut;
}

// output changing at control rate, rest audio
void DigitalIO_next_aaka_once(DigitalIO* unit, int inNumSamples) {
    World* world = unit->mWorld;
    BelaContext* context = world->mBelaContext;

    float* pinid = IN(0);
    float in = IN0(1); // input value
    float* iomode = IN(2); // IO mode : < 0.5 = input, else output
    float* out = ZOUT(0); // output value = last output value

    float newmode = 0; // input

    int newDigInInt = unit->mLastDigitalIn;
    float newDigIn = (float)newDigInInt;
    //   int newDigOut = unit->mLastDigitalOut;
    int newDigOut = (int)in;

    for (unsigned int n = 0; n < inNumSamples; n++) {
        // read input
        if (DigitalIO_updatePin(unit, pinid[n])) {
            newmode = iomode[n];
            if (newmode < 0.5) {
                pinModeOnce(context, n, unit->mDigitalPin, INPUT);
                newDigInInt = digitalRead(context, n, unit->mDigitalPin);
            } else {
                pinModeOnce(context, n, unit->mDigitalPin, OUTPUT);
                digitalWriteOnce(context, n, unit->mDigitalPin, newDigOut);
            }
        }
        // always write to the output of the UGen
        *++out = (float)newDigInInt;
    }
    unit->mLastDigitalIn = newDigInInt;
    unit->mLastDigitalOut = newDigOut;
}


// output changing at control rate, and pin mode at control rate
void DigitalIO_next_aakk_once(DigitalIO* unit, int inNumSamples) {
//THIS ONE
    World* world = unit->mWorld;
    BelaContext* context = world->mBelaContext;

    float* pinid = IN(0);
    float in = IN0(1); // input value
    float iomode = IN0(2); // IO mode : < 0.5 = input, else output
    float* out = ZOUT(0); // output value = last output value

    int newDigInInt = unit->mLastDigitalIn;
    float newDigIn = (float)newDigInInt;
    //   int newDigOut = unit->mLastDigitalOut;
    int newDigOut = (int)in;

    if (iomode < 0.5) {
        for (unsigned int n = 0; n < inNumSamples; n++) {
            if (DigitalIO_updatePin(unit, pinid[n])) {
                pinModeOnce(context, n, unit->mDigitalPin, INPUT);
                newDigInInt = digitalRead(context, n, unit->mDigitalPin);
            }
            // always write to the output of the UGen
            *++out = (float)newDigInInt;
        }
    } else {
        for (unsigned int n = 0; n < inNumSamples; n++) {
            if (DigitalIO_updatePin(unit, pinid[n])) {
                pinModeOnce(context, n, unit->mDigitalPin, OUTPUT);
                digitalWriteOnce(context, n, unit->mDigitalPin, newDigOut);
            }
            *++out = (float)newDigInInt;
        }
    }
    unit->mLastDigitalIn = newDigInInt;
    unit->mLastDigitalOut = newDigOut;
}


// pin changing at control rate, output control rate, rest audio rate
void DigitalIO_next_akaa_once(DigitalIO* unit, int inNumSamples) {
    World* world = unit->mWorld;
    BelaContext* context = world->mBelaContext;

    float pinid = IN0(0);
    float in = IN0(1); // input value
    float* iomode = IN(2); // IO mode : < 0.5 = input, else output
    float* out = ZOUT(0); // output value = last output value

    int newDigInInt = unit->mLastDigitalIn;
    float newDigIn = (float)newDigInInt;

    int newDigOut = (int)in;

    if (DigitalIO_updatePin(unit, pinid)) {
        for (unsigned int n = 0; n < inNumSamples; n++) {
            // 	  newinput = in[n];
            float newmode = iomode[n];
            if (newmode < 0.5) {
                pinModeOnce(context, n, unit->mDigitalPin, INPUT);
                newDigInInt = digitalRead(context, n, unit->mDigitalPin);
            } else {
                pinModeOnce(context, n, unit->mDigitalPin, OUTPUT);
                digitalWriteOnce(context, n, unit->mDigitalPin, newDigOut);
            }
            // always write to the output of the UGen
            *++out = (float)newDigInInt;
        }
    }
    unit->mLastDigitalIn = newDigInInt;
    unit->mLastDigitalOut = newDigOut;
}

// result audio rate, pin changing at control rate, output value audio rate, pin mode change control rate
void DigitalIO_next_akak_once(DigitalIO* unit, int inNumSamples) {
    World* world = unit->mWorld;
    BelaContext* context = world->mBelaContext;

    float pinid = IN0(0);
    float* in = IN(1); // input value
    float iomode = IN0(2); // IO mode : < 0.5 = input, else output
    float* out = ZOUT(0); // output value = last output value

    int newDigInInt = unit->mLastDigitalIn;
    float newDigIn = (float)newDigInInt;

    int newDigOut = (int)in;

    if (DigitalIO_updatePin(unit, pinid)) {
        if (iomode < 0.5) {
            pinMode(context, 0, unit->mDigitalPin, INPUT);
            for (unsigned int n = 0; n < inNumSamples; n++) {
                newDigInInt = digitalRead(context, n, unit->mDigitalPin);
                // always write to the output of the UGen
                *++out = (float)newDigInInt;
            }
        } else {
            pinMode(context, 0, unit->mDigitalPin, OUTPUT);
            for (unsigned int n = 0; n < inNumSamples; n++) {
                newDigOut = (int)in[n];
                digitalWriteOnce(context, n, unit->mDigitalPin, newDigOut);
                // always write to the output of the UGen
                *++out = (float)newDigInInt;
            }
        }
    }

    unit->mLastDigitalIn = newDigInInt;
    unit->mLastDigitalOut = newDigOut;
}

// audio rate ugen output, pin changing at control rate, output at control rate, mode at audio rate
void DigitalIO_next_akka_once(DigitalIO* unit, int inNumSamples) {
    World* world = unit->mWorld;
    BelaContext* context = world->mBelaContext;

    float pinid = IN0(0);
    float in = IN0(1); // input value
    float* iomode = IN(2); // IO mode : < 0.5 = input, else output
    float* out = ZOUT(0); // output value = last output value

    float newinput = in;

    int newDigInInt = unit->mLastDigitalIn;
    int newDigOut = unit->mLastDigitalOut;

    bool shouldDo = DigitalIO_updatePin(unit, pinid);

    for (unsigned int n = 0; n < inNumSamples; n++) {
        if(shouldDo) {
            float newmode = iomode[n];
            if (newmode < 0.5) { // digital read
                pinModeOnce(context, n, unit->mDigitalPin, INPUT);
                newDigInInt = digitalRead(context, n, unit->mDigitalPin);
            } else { // digital write
                pinModeOnce(context, n, unit->mDigitalPin, OUTPUT);
                if (newinput > 0.5) {
                    newDigOut = 1;
                } else {
                    newDigOut = 0;
                }
                digitalWriteOnce(context, n, unit->mDigitalPin, newDigOut);
            }
        }
        // always write to the output of the UGen
        *++out = (float)newDigInInt;
    }
    unit->mLastDigitalIn = newDigInInt;
    unit->mLastDigitalOut = newDigOut;
}


// all inputs at control rate, output at audio rate
void DigitalIO_next_ak(DigitalIO* unit, int inNumSamples) {
    World* world = unit->mWorld;
    BelaContext* context = world->mBelaContext;

    float pinid = IN0(0);
    float in = IN0(1); // input value
    float iomode = IN0(2); // IO mode : < 0.5 = input, else output
    float* out = ZOUT(0); // output value = last output value

    int newDigInInt = unit->mLastDigitalIn;
    float newDigIn = (float)newDigInInt;
    int newDigOut = (int)in;

    if (DigitalIO_updatePin(unit, pinid)) {
        if (iomode < 0.5) {
            pinMode(context, 0, unit->mDigitalPin, INPUT);
            for (unsigned int n = 0; n < inNumSamples; n++) {
                // read input
                newDigInInt = digitalRead(context, n, unit->mDigitalPin);
                // always write to the output of the UGen
                *++out = (float)newDigInInt;
            }
        } else {
            pinMode(context, 0, unit->mDigitalPin, OUTPUT);
            for (unsigned int n = 0; n < inNumSamples; n++) {
                if (in > 0.5) {
                    newDigOut = 1;
                } else {
                    newDigOut = 0;
                }
                digitalWriteOnce(context, n, unit->mDigitalPin, newDigOut);
                // always write to the output of the UGen
                *++out = (float)newDigInInt;
            }
        }
    }
    unit->mLastDigitalIn = newDigInInt;
    unit->mLastDigitalOut = newDigOut;
}

// all at control rate, output at control rate
void DigitalIO_next_kk(DigitalIO* unit, int inNumSamples) {
    World* world = unit->mWorld;
    BelaContext* context = world->mBelaContext;

    int pinid = (int)IN0(0);
    for(unsigned int n = 0; n < 3; ++n)
        rt_printf("%d ", (int)IN(0)[n]);
    rt_printf("_\n");
    float in = IN0(1); // input value
    float iomode = IN0(2); // IO mode : < 0.5 = input, else output

    int newDigInInt = unit->mLastDigitalIn;
    int newDigOut = unit->mLastDigitalOut;

    if (DigitalIO_updatePin(unit, pinid)) {
        if (iomode < 0.5) {
            pinMode(context, 0, unit->mDigitalPin, INPUT);
            newDigInInt = digitalRead(context, 0, unit->mDigitalPin);
        } else {
            pinMode(context, 0, unit->mDigitalPin, OUTPUT);
            if (in > 0.5) {
                newDigOut = 1;
            } else {
                newDigOut = 0;
            }
            digitalWrite(context, 0, unit->mDigitalPin, newDigOut);
        }
    }
    ZOUT0(0) = (float)newDigInInt;

    unit->mLastDigitalIn = newDigInInt;
    unit->mLastDigitalOut = newDigOut;
}

static int parseDigitalValue(float value) {
    return value > 0.5;
}

static int parseDigitalMode(float mode)
{
    if(mode < 0.5)
        return INPUT;
    else
        return OUTPUT;
}

void DigitalIO_next_universal(DigitalIO* unit, int inNumSamples) {
    World* world = unit->mWorld;
    BelaContext* context = world->mBelaContext;
    const bool ugenAudioRate = (calc_FullRate == unit->mCalcRate);
    const bool pinAudioRate = (calc_FullRate == INRATE(0));
    const bool inputAudioRate = (calc_FullRate == INRATE(1));
    const bool modeAudioRate = (calc_FullRate == INRATE(2));

    unsigned int outsCount = ugenAudioRate ? inNumSamples : 1;
    unsigned int pinsCount = pinAudioRate ? inNumSamples : 1;
    unsigned int insCount = inputAudioRate ? inNumSamples : 1;
    unsigned int modesCount = modeAudioRate ? inNumSamples : 1;
    AccessBufferWriter outs(OUT(0), outsCount); // may be the same as pins
    const AccessBufferReader pins(IN(0), pinsCount);
    const AccessBufferReader ins(IN(1), insCount);
    const AccessBufferReader modes(IN(2), modesCount);

    bool lastDigIn = unit->mLastDigitalIn;
    // with properly initialised AccessBuffers, we can use [n] below regardless
    // of the K/A rate of each buffer
    for(unsigned int n = 0; n < inNumSamples; ++n) {
        unsigned int pin = (int)pins[n];
        if (DigitalIO_updatePin(unit, pin)) {
            int mode = parseDigitalMode(modes[n]);
            if(1 == inNumSamples) {
                // we are only ever going to go up to 1 (i.e.: processed at
                // control rate). So fill up the rest of the buffer.
                pinMode(context, 0, unit->mDigitalPin, mode);
            } else {
                pinModeOnce(context, n, unit->mDigitalPin, mode);
            }
            if(INPUT == mode) {
                lastDigIn = digitalRead(context, n, unit->mDigitalPin);
            } else {
                bool digOut = parseDigitalValue(ins[n]);
                if(1 == inNumSamples) {
                    // we are only ever going to go up to 1 (i.e.: processed at
                    // control rate). So fill up the rest of the buffer.
                    digitalWrite(context, 0, unit->mDigitalPin, digOut);
                } else {
                    digitalWriteOnce(context, n, unit->mDigitalPin, digOut);
                }
            }
            outs[n] = lastDigIn;
        }
    }
    unit->mLastDigitalIn = lastDigIn;
}

void DigitalIO_Ctor(DigitalIO* unit) {
    BelaContext* context = unit->mWorld->mBelaContext;

    unit->mDigitalPin = 0;
    unit->mLastDigitalIn = 0;
    unit->mLastDigitalOut = 0;
#if 0
    SETCALC(DigitalIO_next_universal);
#else
    // set calculation method
    if (unit->mCalcRate == calc_FullRate) { // ugen running at audio rate;
        if (INRATE(0) == calc_FullRate) { // pin changed at audio rate
            if (INRATE(1) == calc_FullRate) { // output changed at audio rate
                if (INRATE(2) == calc_FullRate) { // pinmode changed at audio rate
                    //                         if ( writeMode ){
                    //                             rt_printf("DigitalIO: aaaa once\n");
                    SETCALC(DigitalIO_next_aaaa_once);
                    //                         } else {
                    //                             SETCALC(DigitalIO_next_aaaa);
                    //                         }
                } else {
                    //                         if ( writeMode ){
                    //                             rt_printf("DigitalIO: aaak once\n");
                    SETCALC(DigitalIO_next_aaak_once);
                    //                         } else {
                    //                             SETCALC(DigitalIO_next_aaak);
                    //                         }
                }
            } else { // output changed at control rate
                if (INRATE(2) == calc_FullRate) { // pinmode changed at audio rate
                    //                         if ( writeMode ){
                    //                             rt_printf("DigitalIO: aaka once\n");
                    SETCALC(DigitalIO_next_aaka_once);
                    //                         } else {
                    //                             SETCALC(DigitalIO_next_aaka);
                    //                         }
                } else {
                    //                         if ( writeMode ){
                    //                             rt_printf("DigitalIO: aakk once\n");
                    SETCALC(DigitalIO_next_aakk_once);
                    //                         } else {
                    //                             SETCALC(DigitalIO_next_aakk);
                    //                         }
                }
            }
        } else { // pin changed at control rate
            if (INRATE(1) == calc_FullRate) { // output changed at audio rate
                if (INRATE(2) == calc_FullRate) { // pinmode changed at audio rate
                    //                         if ( writeMode ){
                    //                             rt_printf("DigitalIO: akaa once\n");
                    SETCALC(DigitalIO_next_akaa_once);
                    //                         } else {
                    //                             SETCALC(DigitalIO_next_akaa);
                    //                         }
                } else {
                    //                         if ( writeMode ){
                    //                             rt_printf("DigitalIO: akak once\n");
                    SETCALC(DigitalIO_next_akak_once);
                    //                         } else {
                    //                             SETCALC(DigitalIO_next_akak);
                    //                         }
                }
            } else { // output changed at control rate
                if (INRATE(2) == calc_FullRate) { // pinmode changed at audio rate
                    //                         if ( writeMode ){
                    //                             rt_printf("DigitalIO: akka once\n");
                    SETCALC(DigitalIO_next_akka_once);
                    //                         } else {
                    //                             SETCALC(DigitalIO_next_akka);
                    //                         }
                } else { // pinmode at control rate
                    //                         rt_printf("DigitalIO: ak once\n");
                    SETCALC(DigitalIO_next_ak);
                }
            }
        }
    } else { // ugen at control rate
        if ((INRATE(0) == calc_FullRate) || (INRATE(1) == calc_FullRate) || (INRATE(2) == calc_FullRate)) {
            rt_fprintf(stderr, "DigitalIO warning: UGen rate is control rate, so cannot change inputs at audio rate\n");
        }
        //             rt_printf("DigitalIO: kk\n");
        SETCALC(DigitalIO_next_kk);
    }
#endif
    BelaUgen_init_output(unit);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

struct BelaScopeOut : public Unit {
    unsigned int maxScopeChannels;
    unsigned int numScopeChannels;
    unsigned int offset;
    unsigned int scopeBufferSamples;
};

void BelaScopeOut_next(BelaScopeOut *unit, unsigned int numSamples) {
    float *scopeBuffer = unit->mWorld->mBelaScope->buffer;
    if(!scopeBuffer) return;
    unsigned int numChannels = unit->numScopeChannels;
    unsigned int maxChannels = unit->maxScopeChannels;
    unsigned int scopeBufferSamples = unit->scopeBufferSamples;
    float* inputPointers[numChannels];
    for (unsigned int ch = 0; ch < numChannels; ++ch)
        inputPointers[ch] = ZIN(ch + 1);

    for(unsigned int frame = unit->offset; frame < scopeBufferSamples; frame+=maxChannels)
        for (unsigned int ch = 0; ch < numChannels; ++ch) scopeBuffer[frame+ch] += ZXP(inputPointers[ch]);
    unit->mWorld->mBelaScope->touched = true;
}

void BelaScopeOut_noop(BelaScopeOut *unit) { /*noop*/ }

void BelaScopeOut_Ctor(BelaScopeOut *unit) {
    BelaScope* scope = unit->mWorld->mBelaScope;
    if(!scope || !scope->buffer) {
        rt_fprintf(stderr, "BelaScopeOut error: Scope not initialized on server\n");
        SETCALC(BelaScopeOut_noop);
        return;
    };
    unit->offset = ZIN0(0);
    unit->scopeBufferSamples = unit->mWorld->mBelaScope->bufferSamples;
    uint32 maxScopeChannels = unit->mWorld->mBelaMaxScopeChannels;
    uint32 numInputSignals = unit->mNumInputs - 1;
    unit->numScopeChannels = sc_min(numInputSignals, maxScopeChannels);
    unit->maxScopeChannels = maxScopeChannels;
    // TODO: check valid offset
    if (numInputSignals > maxScopeChannels) {
        rt_fprintf(stderr,
                   "BelaScopeOut warning: can't scope %i channels, maxBelaScopeChannels is set to %i\n",
                   numInputSignals, maxScopeChannels);
    }
    
    BelaScopeOut_next(unit, 1);
    SETCALC(BelaScopeOut_next);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

PluginLoad(BELA) {
    ft = inTable;

    DefineSimpleUnit(MultiplexAnalogIn);
    DefineSimpleUnit(AnalogIn);
    DefineSimpleUnit(AnalogOut);
    DefineSimpleUnit(DigitalIn);
    DefineSimpleUnit(DigitalOut);
    DefineSimpleUnit(DigitalIO);
    DefineSimpleUnit(BelaScopeOut);
}


// C_LINKAGE SC_API_EXPORT void unload(InterfaceTable *inTable)
// {
//
// }
o