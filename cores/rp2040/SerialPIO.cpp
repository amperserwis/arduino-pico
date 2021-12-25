/*
    Serial-over-PIO for the Raspberry Pi Pico RP2040

    Copyright (c) 2021 Earle F. Philhower, III <earlephilhower@yahoo.com>

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

#include "SerialPIO.h"
#include "CoreMutex.h"
#include <hardware/gpio.h>
#include <map>
#include "pio_uart.pio.h"


// ------------------------------------------------------------------------
// -- Generates a unique program for differing bit lengths
static std::map<int, PIOProgram*> _txMap;
static std::map<int, PIOProgram*> _rxMap;

// Duplicate a program and replace the first insn with a "set x, repl"
static pio_program_t *pio_make_uart_prog(int repl, const pio_program_t *pg) {
    pio_program_t *p = new pio_program_t;
    p->length = pg->length;
    p->origin = pg->origin;
    uint16_t *insn = (uint16_t *)malloc(p->length * 2);
    memcpy(insn, pg->instructions, p->length * 2);
    insn[0] = pio_encode_set(pio_x, repl);
    p->instructions = insn;
    return p;
}

static PIOProgram *_getTxProgram(int bits) {
    auto f = _txMap.find(bits);
    if (f == _txMap.end()) {
        pio_program_t * p = pio_make_uart_prog(bits, &pio_tx_program);
        _txMap.insert({bits, new PIOProgram(p)});
        f = _txMap.find(bits);
    }
    return f->second;
}

static PIOProgram *_getRxProgram(int bits) {
    auto f = _rxMap.find(bits);
    if (f == _rxMap.end()) {
        pio_program_t * p = pio_make_uart_prog(bits, &pio_rx_program);
        _rxMap.insert({bits, new PIOProgram(p)});
        f = _rxMap.find(bits);
    }
    return f->second;
}
// ------------------------------------------------------------------------

static int _parity(int bits, int data) {
    int p = 0;
    for (int b = 0; b < bits; b++) {
        p ^= (data & (1<<b)) ? 1 : 0;
    }
    return p;
}


bool SerialPIO::setRX(pin_size_t pin) {
    if (!_running) {
        _rx = pin;
        return true;
    }

    panic("FATAL: Attempting to set SerialPIO.RX while running");
    return false;
}

bool SerialPIO::setTX(pin_size_t pin) {
    if (!_running) {
        _tx = pin;
        return true;
    }

    panic("FATAL: Attempting to set SerialPIO.TX while running");
    return false;
}

SerialPIO::SerialPIO(pin_size_t tx, pin_size_t rx) {
    _tx = tx;
    _rx = rx;
    mutex_init(&_mutex);
}

void SerialPIO::begin(unsigned long baud, uint16_t config) {
    _baud = baud;
    switch (config & SERIAL_PARITY_MASK) {
        case SERIAL_PARITY_EVEN:
            _parity = UART_PARITY_EVEN;
            break;
        case SERIAL_PARITY_ODD:
            _parity = UART_PARITY_ODD;
            break;
        default:
            _parity = UART_PARITY_NONE;
            break;
    }
    switch (config & SERIAL_STOP_BIT_MASK) {
        case SERIAL_STOP_BIT_1:
            _stop = 1;
            break;
        default:
            _stop = 2;
            break;
    }
    switch (config & SERIAL_DATA_MASK) {
        case SERIAL_DATA_5:
            _bits = 5;
            break;
        case SERIAL_DATA_6:
            _bits = 6;
            break;
        case SERIAL_DATA_7:
            _bits = 7;
            break;
        default:
            _bits = 8;
            break;
    }

    if ((_tx == -1) && (_rx == -1)) {
        DEBUGCORE("ERROR: No pins specified for SerialPIO\n");
        return;
    }

    if (_tx != -1) {
        _txBits = _bits + _stop + (_parity != UART_PARITY_NONE ? 1 : 0) + 1/*start bit*/;
        _txPgm = _getTxProgram(_txBits);
        int off;
        if (!_txPgm->prepare(&_txPIO, &_txSM, &off)) {
            DEBUGCORE("ERROR: Unable to allocate PIO TX UART, out of PIO resources\n");
            // ERROR, no free slots
            return;
        }

        pinMode(_tx, OUTPUT);
    
        pio_tx_program_init(_txPIO, _txSM, off, _tx);
        pio_sm_clear_fifos(_txPIO, _txSM); // Remove any existing data

        // Put the divider into ISR w/o using up program space
        pio_sm_put_blocking(_txPIO, _txSM, clock_get_hz(clk_sys) / _baud - 2);
        pio_sm_exec(_txPIO, _txSM, pio_encode_pull(false, false));
        pio_sm_exec(_txPIO, _txSM, pio_encode_mov(pio_isr, pio_osr));

        // Start running!
        pio_sm_set_enabled(_txPIO, _txSM, true);
    }
    if (_rx != -1) {
        _rxBits = 2 * (_bits + _stop + (_parity != UART_PARITY_NONE ? 1 : 0) + 1);
        _rxPgm = _getRxProgram(_rxBits);
        int off;
        if (!_rxPgm->prepare(&_rxPIO, &_rxSM, &off)) {
            DEBUGCORE("ERROR: Unable to allocate PIO RX UART, out of PIO resources\n");
            return;
        }
        pinMode(_rx, INPUT);
        pio_rx_program_init(_rxPIO, _rxSM, off, _rx);
        pio_sm_clear_fifos(_rxPIO, _rxSM); // Remove any existing data

        // Put phase divider into OSR w/o using add'l program memory
        pio_sm_put_blocking(_rxPIO, _rxSM, clock_get_hz(clk_sys) / (_baud * 2) - 2);
        pio_sm_exec(_rxPIO, _rxSM, pio_encode_pull(false, false));

        pio_sm_set_enabled(_rxPIO, _rxSM, true);
    }

    _running = true;
    while (!_swFIFO.empty()) {
        (void)_swFIFO.pop(); // Just throw out anything in our old FIFO
    }
}

void SerialPIO::end() {
    if (!_running) {
        return;
    }
    // TODO: Deallocate PIO resources, stop them
    _running = false;
}

// Transfers any data in the HW FIFO into our SW one, up to 32 bytes
void SerialPIO::_pumpFIFO() {
    if (_rx == -1) {
        return;
    }
    while ((_swFIFO.size() < 32) && (!pio_sm_is_rx_fifo_empty(_rxPIO, _rxSM))) {
        uint32_t decode = _rxPIO->rxf[_rxSM];
        decode >>= 32 - _rxBits;
        uint32_t val = 0;
        for (int b = 0; b < _bits + 1; b++) {
            val |= (decode & (1<<(b*2))) ? 1<<b : 0;
        }
        if (_parity == UART_PARITY_EVEN) {
            int p = ::_parity(_bits, val);
            int r = (val & (1 << _bits)) ? 1 : 0;
            if (p != r) {
                // TODO - parity error
                continue;
            }
        } else if (_parity == UART_PARITY_ODD) {
            int p = ::_parity(_bits, val);
            int r = (val & (1 << _bits)) ? 1 : 0;
            if (p == r) {
                // TODO - parity error
                continue;
            }
        }
        _swFIFO.push(val & ((1 << _bits) -  1));
    }
}

int SerialPIO::peek() {
    CoreMutex m(&_mutex);
    if (!_running || !m || (_rx == -1)) {
        return -1;
    }
    _pumpFIFO();
    // If there's something in the FIFO now, just peek at it
    if (_swFIFO.size()) {
        return _swFIFO.front();
    }
    return -1; // Nothing available before timeout
}

int SerialPIO::read() {
    CoreMutex m(&_mutex);
    if (!_running || !m || (_rx == -1)) {
        return -1;
    }
    unsigned int start = millis();
    unsigned int end = millis();
    while ((end - start) < _timeout) {
        _pumpFIFO();
        if (_swFIFO.size()) {
            auto ret = _swFIFO.front();
            _swFIFO.pop();
            return ret;
        }
        end = millis();
    }
    return -1; // Timeout
}

int SerialPIO::available() {
    CoreMutex m(&_mutex);
    if (!_running || !m || (_rx == -1)) {
        return 0;
    }
    _pumpFIFO();
    return _swFIFO.size();
}

int SerialPIO::availableForWrite() {
    CoreMutex m(&_mutex);
    if (!_running || !m || (_tx == -1)) {
        return 0;
    }
    _pumpFIFO();
    return 8 - pio_sm_get_tx_fifo_level(_txPIO, _txSM);
}

void SerialPIO::flush() {
    CoreMutex m(&_mutex);
    if (!_running || !m || (_tx == -1)) {
        return;
    }
    _pumpFIFO();
    while (!pio_sm_is_tx_fifo_empty(_txPIO, _txSM)) {
        delay(1); // Wait for all FIFO to be read
    }
    // Could have 1 byte being transmitted, so wait for bit times
   delay((1000 * (_txBits + 1)) / _baud);
}

size_t SerialPIO::write(uint8_t c) {
    CoreMutex m(&_mutex);
    if (!_running || !m || (_tx == -1)) {
        return 0;
    }
    _pumpFIFO();

    uint32_t val = c;
    if (_parity == UART_PARITY_NONE) {
        val |= 3 << _bits; // Set 2 stop bits, the HW will only transmit the required number
    } else if (_parity == UART_PARITY_EVEN) {
        val |= ::_parity(_bits, c) << _bits;
        val |= 3 << (_bits + 1);
    } else {
        val |= (1 ^ ::_parity(_bits, c)) << _bits;
        val |= 3 << (_bits + 1);
    }
    val <<= 1;  // Start bit = low
    pio_sm_put_blocking(_txPIO, _txSM, val);

    return 1;
}

size_t SerialPIO::write(const uint8_t *p, size_t len) {
    CoreMutex m(&_mutex);
    if (!_running || !m || (_tx == -1)) {
        return 0;
    }
    size_t cnt = len;
    while (cnt) {
        _pumpFIFO();
        write(*p);
        cnt--;
        p++;
    }
    return len;
}

SerialPIO::operator bool() {
    return _running;
}
