/*
    I2SIn and I2SOut for Raspberry Pi Pico
    Implements one or more I2S interfaces using DMA

    Copyright (c) 2022 Earle F. Philhower, III <earlephilhower@yahoo.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include <Arduino.h>
#include "I2S.h"
#include "pio_i2s.pio.h"


I2S::I2S(PinMode direction) {
    _running = false;
    _bps = 16;
    _writtenHalf = false;
    _pinBCLK = 26;
    _pinDOUT = 28;
    _freq = 48000;
    _arb = nullptr;
    _isOutput = direction == OUTPUT;
}

I2S::~I2S() {
}

bool I2S::setBCLK(pin_size_t pin) {
    if (_running || (pin > 28)) {
        return false;
    }
    _pinBCLK = pin;
    return true;
}

bool I2S::setDATA(pin_size_t pin) {
    if (_running || (pin > 29)) {
        return false;
    }
    _pinDOUT = pin;
    return true;
}

bool I2S::setBitsPerSample(int bps) {
    if (_running || ((bps != 8) && (bps != 16) && (bps != 24) && (bps != 32))) {
        return false;
    }
    _bps = bps;
    return true;
}

bool I2S::setFrequency(int newFreq) {
    _freq = newFreq;
    if (_running) {
        float bitClk = _freq * _bps * 2.0;
        pio_sm_set_clkdiv(_pio, _sm, (float)clock_get_hz(clk_sys) / bitClk);
    }
    return true;
}

bool I2S::begin() {
    _running = true;
    int off = 0;
    _i2s = new PIOProgram(_isOutput ? &pio_i2s_out_program : &pio_i2s_in_program);
    _i2s->prepare(&_pio, &_sm, &off);
    if (_isOutput) {
        pio_i2s_out_program_init(_pio, _sm, off, _pinDOUT, _pinBCLK, _bps);
    } else {
        pio_i2s_in_program_init(_pio, _sm, off, _pinDOUT, _pinBCLK, _bps);
    }
    setFrequency(_freq);
    _arb = new AudioRingBuffer(8, 8, 32, 0, _isOutput ? OUTPUT : INPUT);
    _arb->begin(pio_get_dreq(_pio, _sm, _isOutput), _isOutput ? &_pio->txf[_sm] : (volatile void*)&_pio->rxf[_sm]);
    pio_sm_set_enabled(_pio, _sm, true);

    return true;
}

void I2S::end() {
    _running = false;
    delete _arb;
    _arb = nullptr;
    delete _i2s;
    _i2s = nullptr;
}

int I2S::available() {
    return -1;
}
int I2S::read() {
    return -1;
}
int I2S::peek() {
    return -1;
}
void I2S::flush() {
}

size_t I2S::write(uint32_t val, bool sync) {
    if (!_running || !_isOutput) {
        return 0;
    }
    return _arb->write(val, sync);
}

size_t I2S::write8(uint8_t l, uint8_t r) {
    if (!_running || !_isOutput) {
        return 0;
    }
    uint16_t o = (l << 8) | r;
    _holdWord <<= 16;
    _holdWord |= o;
    if (_wasHolding) {
        _wasHolding = false;
        write((uint32_t)_holdWord, true);
    } else {
        _wasHolding = true;
    }
    return 1;
}

size_t I2S::write16(uint16_t l, uint16_t r) {
    if (!_running || !_isOutput) {
        return 0;
    }
    uint32_t o = (l << 16) | r;
    return write((uint32_t)o, true);
}

size_t I2S::write24(uint32_t l, uint32_t r) {
    return write32(l, r);
}

size_t I2S::write32(uint32_t l, uint32_t r) {
    if (!_running || !_isOutput) {
        return 0;
    }
    write((uint32_t)l);
    write((uint32_t)r);
    return 1;
}

size_t I2S::read(uint32_t *val, bool sync) {
    if (!_running || _isOutput) {
        return 0;
    }
    return _arb->read(val, sync);
}

bool I2S::read8(uint8_t *l, uint8_t *r) {
    if (!_running || _isOutput) {
        return false;
    }
    if (_wasHolding) {
        *l = (_holdWord >> 8) & 0xff;
        *r = (_holdWord >> 0) & 0xff;
        _wasHolding = false;
    } else {
        read(&_holdWord, true);
        _wasHolding = true;
        *l = (_holdWord >> 24) & 0xff;
        *r = (_holdWord >> 16) & 0xff;
    }
    return true;
}

bool I2S::read16(uint16_t *l, uint16_t *r) {
    if (!_running || _isOutput) {
        return false;
    }
    uint32_t o;
    read(&o, true);
    *l = (o >> 16) & 0xffff;
    *r = (o >> 0) & 0xffff;
    return true;
}

bool I2S::read24(uint32_t *l, uint32_t *r) {
    if (!_running || _isOutput) {
        return false;
    }
    read32(l, r);
    // 24-bit samples are read right-aligned, so left-align them to keep the binary point between 33.32
    *l <<= 8;
    *r <<= 8;
    return true;
}

bool I2S::read32(uint32_t *l, uint32_t *r) {
    if (!_running || _isOutput) {
        return false;
    }
    read(l, true);
    read(r, true);
    return true;
}


size_t I2S::write(uint8_t) {
    return 0;
}
size_t I2S::write(const uint8_t *buffer, size_t size) {
    (void) buffer;
    (void) size;
    return 0;
}
int I2S::availableForWrite() {
    return 1;
}
