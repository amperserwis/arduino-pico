/*
    KeyboardBT.cpp

    Modified by Earle F. Philhower, III <earlephilhower@yahoo.com>
    Main Arduino Library Copyright (c) 2015, Arduino LLC
    Original code (pre-library): Copyright (c) 2011, Peter Barrett

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

#include "KeyboardBT.h"
#include "KeyboardLayout.h"
#include <PicoBluetoothHID.h>

//================================================================================
//================================================================================
//  Keyboard

KeyboardBT_::KeyboardBT_(void) {
    bzero(&_keyReport, sizeof(_keyReport));
    _asciimap = KeyboardLayout_en_US;
}

#define REPORT_ID 0x01

static const uint8_t desc_keyboard[] = {TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID))};
void KeyboardBT_::begin(const uint8_t *layout) {
    _asciimap = layout;
    PicoBluetoothHID.startHID("PicoW Keyboard 00:00:00:00:00:00", "PicoW HID Keyboard", 0x2540, 33, desc_keyboard, sizeof(desc_keyboard));
}

void KeyboardBT_::end(void) {
    PicoBluetoothHID.end();
}

void KeyboardBT_::sendReport(KeyReport* keys) {
    hid_keyboard_report_t data;
    data.modifier = keys->modifiers;
    data.reserved = 0;
    memcpy(data.keycode, keys->keys, sizeof(data.keycode));
    PicoBluetoothHID.send(REPORT_ID, &data, sizeof(data));
}

KeyboardBT_ KeyboardBT;
