
/******************************************************************************

    written by stefan krueger (s-light),
        github@s-light.eu, http://s-light.eu, https://github.com/s-light/

******************************************************************************/
/******************************************************************************
    The MIT License (MIT)

    Copyright (c) 2018 Stefan Krüger

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
******************************************************************************/


// include Core Arduino functionality
#include <Arduino.h>

// include SPI library
#include <SPI.h>

// include own headerfile
#include "slight_TLC5957.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// register definitions
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~




static const struct _FC_FIELDS_t {
    const slight_TLC5957::function_control_t LODVTH = {
        .offset = 0,
        .length = 2,
        .mask = 0b11,
        .defaultv = 0b01,
    };
    const slight_TLC5957::function_control_t SEL_TD0 = {
        .offset = 2,
        .length = 2,
        .mask = 0b11,
        .defaultv = 0b01,
    };
    const slight_TLC5957::function_control_t SEL_GDLY = {
        .offset = 4,
        .length = 1,
        .mask = 0b1,
        .defaultv = 0b1,
    };
    const slight_TLC5957::function_control_t XREFRESH = {
        .offset = 5,
        .length = 1,
        .mask = 0b1,
        .defaultv = 0b0,
    };
    const slight_TLC5957::function_control_t SEL_GCK_EDGE = {
        .offset = 6,
        .length = 1,
        .mask = 0b1,
        .defaultv = 0b0,
    };
    const slight_TLC5957::function_control_t SEL_PCHG = {
        .offset = 7,
        .length = 1,
        .mask = 0b1,
        .defaultv = 0b0,
    };
    const slight_TLC5957::function_control_t ESPWM = {
        .offset = 8,
        .length = 1,
        .mask = 0b1,
        .defaultv = 0b0,
    };
    const slight_TLC5957::function_control_t LGSE3 = {
        .offset = 9,
        .length = 1,
        .mask = 0b1,
        .defaultv = 0b0,
    };
    const slight_TLC5957::function_control_t LGSE1 = {
        .offset = 11,
        .length = 3,
        .mask = 0b111,
        .defaultv = 0b000,
    };
    const slight_TLC5957::function_control_t CCB = {
        .offset = 14,
        .length = 9,
        .mask = 0b111111111,
        .defaultv = 0b100000000,
    };
    const slight_TLC5957::function_control_t CCG = {
        .offset = 23,
        .length = 9,
        .mask = 0b111111111,
        .defaultv = 0b100000000,
    };
    const slight_TLC5957::function_control_t CCR = {
        .offset = 32,
        .length = 9,
        .mask = 0b111111111,
        .defaultv = 0b100000000,
    };
    const slight_TLC5957::function_control_t BC = {
        .offset = 41,
        .length = 3,
        .mask = 0b111,
        .defaultv = 0b100,
    };
    const slight_TLC5957::function_control_t PokerTransMode = {
        .offset = 44,
        .length = 1,
        .mask = 0b1,
        .defaultv = 0b0,
    };
    const slight_TLC5957::function_control_t LGSE2 = {
        .offset = 45,
        .length = 3,
        .mask = 0b111,
        .defaultv = 0b000,
    };
} _FC_FIELDS;


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// helper
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

uint16_t slight_TLC5957::set_bit_with_mask(
    uint16_t value, uint16_t mask, uint16_t value_new
) {
    // """Set bit with help of mask."""
    // clear
    value &= ~mask;
    if (value_new) {
        // set
        value |= mask;
    }
    return value;
}

uint16_t slight_TLC5957::set_bit(
    uint16_t value, uint8_t index, uint16_t value_new
) {
    // """Set bit - return new value.
    //
    // Set the index:th bit of value to 1 if value_new is truthy,
    // else to 0, and return the new value.
    // https://stackoverflow.com/a/12174051/574981
    // """
    // Compute mask, an integer with just bit "index" set.
    uint16_t mask = 1 << index;
    // Clear the bit indicated by the mask (if x is False)
    value &= ~mask;
    if (value_new) {
        // If x was True, set the bit indicated by the mask.
        value |= mask;
    }
    // Return the result, we're done.
    return value;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// functions
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


slight_TLC5957::slight_TLC5957(
    uint16_t pixel_count,
    uint8_t latch,
    uint8_t gsclk,
    uint8_t spi_clock,
    uint8_t spi_mosi,
    uint8_t spi_miso
):
    pixel_count(pixel_count),
    channel_count(pixel_count * COLORS_PER_PIXEL),
    chip_count(
        (pixel_count / PIXEL_PER_CHIP) +
        // check if we pixels for a part of an chip
        ((pixel_count % PIXEL_PER_CHIP) > 0 ? 1 : 0)),
    buffer_byte_count(chip_count * CHIP_GS_BUFFER_BYTE_COUNT),
    buffer(reinterpret_cast<uint16_t*>(calloc(buffer_byte_count, 1))),
    _buffer_fc_byte_count(chip_count * CHIP_BUFFER_BYTE_COUNT),
    _buffer_fc(reinterpret_cast<uint16_t*>(calloc(_buffer_fc_byte_count, 1))),
    _latch(latch),
    _gsclk(gsclk),
    _spi_clock(spi_clock),
    _spi_mosi(spi_mosi),
    _spi_miso(spi_miso
) {
    ready = false;
}

slight_TLC5957::~slight_TLC5957() {
    end();
    free(buffer);
    free(_buffer_fc);
}


void slight_TLC5957::begin() {
    // clean up..
    end();
    // start up...
    if (ready == false) {
        // setup
        pinMode(_latch, OUTPUT);
        // pinMode(_gsclk, OUTPUT);
        pinMode(_spi_miso, INPUT);
        pinMode(_spi_mosi, OUTPUT);
        pinMode(_spi_clock, OUTPUT);
        SPI.begin();
        // SPI.beginTransaction(SPISettings(10 * 1000000, MSBFIRST, SPI_MODE0));
        SPI.beginTransaction(SPISettings(1 * 1000000, MSBFIRST, SPI_MODE0));

        _init_buffer_fc();
        update_fc();

        // write initial 0 values
        show();
        show();
    }
}

void slight_TLC5957::end() {
    if (ready) {
        SPI.endTransaction();
        SPI.end();
    }
}


void slight_TLC5957::_write_buffer_GS() {
    // """Write out grayscale values."""
    uint16_t buffer_start = 0;
    uint16_t write_count = (CHIP_BUFFER_BYTE_COUNT * chip_count)
        - CHIP_FUNCTION_CMD_BYTE_COUNT;

    for (uint8_t pixel_index = 0; pixel_index < PIXEL_PER_CHIP; pixel_index++) {
        // configure
        SPI.beginTransaction(SPISettings(spi_baudrate, MSBFIRST, SPI_MODE0));
        // write GS data for all chips -1*16bit
        // the transfer functions in buffer mode
        // overwrite to the buffer with what comes back in :-(
        // https://www.arduino.cc/en/Reference/SPITransfer
        // SPI.transfer(buffer, write_count);
        // SPI.transfer(reinterpret_cast<uint8_t*>(buffer_start), write_count);
        // so we have two options here.
        // - or we create a copy of the buffer
        // - we do a for loop
        for (uint16_t byte_index = 0; byte_index < write_count; byte_index++) {
            SPI.transfer(buffer[buffer_start + byte_index]);
        }
        SPI.endTransaction();
        buffer_start += write_count;
        // special
        if (pixel_index == (PIXEL_PER_CHIP - 1)) {
            _write_buffer_with_function_command(
                _FC__LATGS, buffer_start, buffer);
        } else {
            _write_buffer_with_function_command(
                _FC__WRTGS, buffer_start, buffer);
        }
        buffer_start += CHIP_FUNCTION_CMD_BYTE_COUNT;
    }
}

void slight_TLC5957::_write_buffer_FC() {
    // """Write out function command values."""
    // Write out the current state to the shift register.
    uint16_t buffer_start = 0;
    uint16_t write_count = (CHIP_BUFFER_BYTE_COUNT * chip_count)
        - CHIP_FUNCTION_CMD_BYTE_COUNT;

    // enable FC write
    _write_buffer_with_function_command(
        _FC__FCWRTEN, buffer_start, _buffer_fc);

    // configure
    SPI.beginTransaction(SPISettings(spi_baudrate, MSBFIRST, SPI_MODE0));
    // write FC data for all chips -1*16bit
    // the transfer functions in buffer mode
    // overwrite to the buffer with what comes back in :-(
    // https://www.arduino.cc/en/Reference/SPITransfer
    // so we do it in a for loop
    for (uint16_t byte_index = 0; byte_index < write_count; byte_index++) {
        SPI.transfer(_buffer_fc[buffer_start + byte_index]);
    }
    SPI.endTransaction();
    buffer_start += write_count;
    // special
    _write_buffer_with_function_command(
        _FC__WRTFC, buffer_start, _buffer_fc);
    // done.
}

void slight_TLC5957::_write_buffer_with_function_command(
    function_command_pulse_count function_command,
    uint16_t buffer_start,
    uint16_t *buffer
) {
    // """Bit-Banging SPI write to sync with latch pulse."""
    uint16_t value = buffer[buffer_start];

    // faster speeds with direct port access:
    // https://forum.arduino.cc/index.php?topic=4324.0

    pinMode(_spi_mosi, OUTPUT);
    pinMode(_spi_clock, OUTPUT);

    digitalWrite(_spi_clock, LOW);
    digitalWrite(_spi_mosi, LOW);
    digitalWrite(_latch, LOW);

    uint8_t latch_start_index = CHIP_FUNCTION_CMD_BIT_COUNT - function_command;

    for (uint8_t i = 0; i < CHIP_FUNCTION_CMD_BIT_COUNT; i++) {
        if (latch_start_index == i) {
            digitalWrite(_latch, HIGH);
        }

        // b1000000000000000
        if (value & 0x8000u) {
            digitalWrite(_spi_mosi, HIGH);
        } else {
            digitalWrite(_spi_mosi, LOW);
        }
        value <<= 1;

        digitalWrite(_spi_clock, HIGH);
        delayMicroseconds(10);
        digitalWrite(_spi_clock, LOW);
    }

    digitalWrite(_latch, LOW);
}

void slight_TLC5957::show() {
    // """Write out Grayscale Values to chips."""
    _write_buffer_GS();
}

void slight_TLC5957::update_fc() {
    // """Write out Function_Command Values to chips."""
    _write_buffer_FC();
}


// ##########################################
// general buffer things


uint64_t slight_TLC5957::_get_48bit_value_from_buffer(
    uint16_t *buffer, uint16_t buffer_start
) {
    return (uint64_t)buffer[buffer_start] & 0xFFFFFFFFFFFF;
    // return (
    //     (buffer[buffer_start + 0] << 40) |
    //     (buffer[buffer_start + 1] << 32) |
    //     (buffer[buffer_start + 2] << 24) |
    //     (buffer[buffer_start + 3] << 16) |
    //     (buffer[buffer_start + 4] <<  8) |
    //      buffer[buffer_start + 5]);
}

void slight_TLC5957::_set_48bit_value_in_buffer(
    uint16_t *buffer, uint16_t buffer_start, uint64_t value
) {
    // uint16_t is by definition > 0.
    if (value <= 0xFFFFFFFFFFFF) {
        // print("buffer_start", buffer_start, "value", value)
        // self._debug_print_buffer()
        buffer[buffer_start] = value & 0xFFFFFFFFFFFF;
        // buffer[buffer_start + 0] = (value >> 40) & 0xFF;
        // buffer[buffer_start + 1] = (value >> 32) & 0xFF;
        // buffer[buffer_start + 2] = (value >> 24) & 0xFF;
        // buffer[buffer_start + 3] = (value >> 16) & 0xFF;
        // buffer[buffer_start + 4] = (value >>  8) & 0xFF;
        // buffer[buffer_start + 5] = value         & 0xFF;
    }
    // else {
    //     raise ValueError(
    //         "value {} not in range: 0..0xFFFFFFFF"
    //         "".format(value)
    //     )
    // }
}


// ##########################################
// FC things

void slight_TLC5957::set_fc_bits_in_buffer(
        uint16_t chip_index,
        uint8_t part_bit_offset,
        const function_control_t *field,
        uint16_t value
) {
    // """Set function control bits in buffer."""
    // print(
    //     "chip_index={} "
    //     "part_bit_offset={} "
    //     "field={} "
    //     "value={} "
    //     "".format(
    //         chip_index,
    //         part_bit_offset,
    //         field,
    //         value
    //     )
    // )
    uint8_t offset = part_bit_offset + field->offset;
    // restrict value
    value &= field->mask;
    // move value to position
    value = value << offset;
    // calculate header start
    uint16_t header_start = chip_index * CHIP_BUFFER_BYTE_COUNT;
    // get chip header
    uint64_t header = _get_48bit_value_from_buffer(
        _buffer_fc, header_start);
    // print("{:048b}".format(header))
    // 0xFFFFFFFFFFFF == 0b11111111111111111111111111111111....
    // create/move mask
    uint16_t mask = field->mask << offset;
    // clear
    header &= ~mask;
    // set
    header |= value;
    // write header back
    _set_48bit_value_in_buffer(_buffer_fc, header_start, header);
}

uint16_t slight_TLC5957::get_fc_bits_in_buffer(
    uint16_t chip_index,
    uint8_t part_bit_offset,
    const function_control_t *field
) {
    // """Get function control bits in buffer."""
    // print(
    //     "chip_index={} "
    //     "part_bit_offset={} "
    //     "field={} "
    //     "".format(
    //         chip_index,
    //         part_bit_offset,
    //         field,
    //     )
    // )
    uint8_t offset = part_bit_offset + field->offset;
    // calculate header start
    uint16_t header_start = chip_index * CHIP_BUFFER_BYTE_COUNT;
    // get chip header
    uint64_t header = _get_48bit_value_from_buffer(
        _buffer_fc, header_start);
    // print("{:048b}".format(header))
    // 0xFFFFFFFFFFFF == 0b11111111111111111111111111111111....
    // create/move mask
    uint16_t mask = field->mask << offset;
    uint16_t value = header & mask;
    // move value to position
    value = value >> offset;
    return value;
}


void slight_TLC5957::_init_buffer_fc() {
    const function_control_t *field;
    for (size_t i = 0; i < chip_count; i++) {
        // for (auto field : _FC_FIELDS) {
        //     set_fc_bits_in_buffer(
        //         i,
        //         field,
        //         field.defaultv);
        // }

        field = &_FC_FIELDS.LODVTH;
        set_fc_bits_in_buffer(i, 0, field, field->defaultv);
        field = &_FC_FIELDS.SEL_TD0;
        set_fc_bits_in_buffer(i, 0, field, field->defaultv);
        field = &_FC_FIELDS.SEL_GDLY;
        set_fc_bits_in_buffer(i, 0, field, field->defaultv);
        field = &_FC_FIELDS.XREFRESH;
        set_fc_bits_in_buffer(i, 0, field, field->defaultv);
        field = &_FC_FIELDS.SEL_GCK_EDGE;
        set_fc_bits_in_buffer(i, 0, field, field->defaultv);
        field = &_FC_FIELDS.SEL_PCHG;
        set_fc_bits_in_buffer(i, 0, field, field->defaultv);
        field = &_FC_FIELDS.ESPWM;
        set_fc_bits_in_buffer(i, 0, field, field->defaultv);
        field = &_FC_FIELDS.LGSE3;
        set_fc_bits_in_buffer(i, 0, field, field->defaultv);
        field = &_FC_FIELDS.LGSE1;
        set_fc_bits_in_buffer(i, 0, field, field->defaultv);
        field = &_FC_FIELDS.CCB;
        set_fc_bits_in_buffer(i, 0, field, field->defaultv);
        field = &_FC_FIELDS.CCG;
        set_fc_bits_in_buffer(i, 0, field, field->defaultv);
        field = &_FC_FIELDS.CCR;
        set_fc_bits_in_buffer(i, 0, field, field->defaultv);
        field = &_FC_FIELDS.BC;
        set_fc_bits_in_buffer(i, 0, field, field->defaultv);
        field = &_FC_FIELDS.PokerTransMode;
        set_fc_bits_in_buffer(i, 0, field, field->defaultv);
        field = &_FC_FIELDS.LGSE2;
        set_fc_bits_in_buffer(i, 0, field, field->defaultv);
    }
}

void slight_TLC5957::print_buffer_fc(Print &out) {
    // TODO(s-light): find a nice way to print this..
    out.println("print_buffer_fc → TODO....");
    // out.println("")
    // result = {}
    // // find longest name
    // // and prepare result
    // max_name_length = 0
    // max_value_bin_length = 0
    // max_value_hex_length = 0
    // for name, content in self._FC_FIELDS.items():
    //     result[name] = []
    //     if max_name_length < len(name):
    //         max_name_length = len(name)
    //     if max_value_bin_length < content["length"]:
    //         max_value_bin_length = content["length"]
    //     mask_as_hex_len = len("{:x}".format(content["mask"]))
    //     if max_value_hex_length < mask_as_hex_len:
    //         max_value_hex_length = mask_as_hex_len
    //
    // // add default
    // for (auto field : _FC_FIELDS) {
    //     result[field_name].append(field.defaultv)
    // }
    //
    // for (size_t i = 0; i < chip_count; i++) {
    //     for (auto field : _FC_FIELDS) {
    //         result[field_name].append(get_fc_bits_in_buffer(i, field));
    //     }
    // }
    //
    // // print
    // ftemp = "{field_name:<" + str(max_name_length)  + "} | "
    // out.print(ftemp.format(field_name = 'name/index'))
    // ftemp = "{field_value:^" + str(max_value_bin_length) + "} | "
    // // ftemp = "{field_value:>" + str(max_value_hex_length) + "} | "
    // out.print(ftemp.format(field_value = 'def'))
    // for index in range(self.chip_count):
    //     ftemp = "{field_value:^" + str(max_value_bin_length) + "} | "
    //     // ftemp = "{field_value:^" + str(max_value_hex_length) + "} | "
    //     out.print(ftemp.format(field_value = index))
    // out.print("")
    // for name, content in result.items():
    //     ftemp = "{field_name:<" + str(max_name_length)  + "} | "
    //     out.print(ftemp.format(field_name = name))
    //     for item in content:
    //         ftemp = "{field_value:>" + str(max_value_bin_length) + "b} | "
    //         // ftemp = "{field_value:>" + str(max_value_hex_length) + "x} | "
    //         out.print(ftemp.format(field_value = item))
    //     out.println("")
}


void slight_TLC5957::set_fc_CC(
    uint16_t chip_index,
    uint16_t CCR_value,
    uint16_t CCG_value,
    uint16_t CCB_value
) {
    set_fc_bits_in_buffer(chip_index, 0, &_FC_FIELDS.CCR, CCR_value);
    set_fc_bits_in_buffer(chip_index, 0, &_FC_FIELDS.CCG, CCG_value);
    set_fc_bits_in_buffer(chip_index, 0, &_FC_FIELDS.CCB, CCB_value);
}

void slight_TLC5957::set_fc_CC_all(
    uint16_t CCR_value,
    uint16_t CCG_value,
    uint16_t CCB_value
) {
    for (size_t chip_index = 0; chip_index < chip_count; chip_index++) {
        set_fc_CC(chip_index, CCR_value, CCG_value, CCB_value);
    }
}

void slight_TLC5957::set_fc_BC(uint16_t chip_index, uint16_t BC_value) {
    set_fc_bits_in_buffer(chip_index, 0, &_FC_FIELDS.BC, BC_value);
}

void slight_TLC5957::set_fc_BC_all(uint16_t BC_value) {
    for (size_t chip_index = 0; chip_index < chip_count; chip_index++) {
        set_fc_BC(chip_index, BC_value);
    }
}

void slight_TLC5957::set_fc_ESPWM(uint16_t chip_index, bool enable) {
    set_fc_bits_in_buffer(chip_index, 0, &_FC_FIELDS.ESPWM, enable);
}

void slight_TLC5957::set_fc_ESPWM_all(bool enable) {
    for (size_t chip_index = 0; chip_index < chip_count; chip_index++) {
        set_fc_ESPWM(chip_index, enable);
    }
}
