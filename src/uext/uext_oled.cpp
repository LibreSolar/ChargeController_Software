/* LibreSolar MPPT charge controller firmware
 * Copyright (c) 2016-2018 Martin Jäger (www.libre.solar)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef UNIT_TEST

#include "main.h"

#ifdef OLED_ENABLED     // otherwise don't compile code to reduce firmware size

#include "uext_oled.h"
#include "pcb.h"

#include "Adafruit_SSD1306.h"

static UExtOled uext_oled; // local instance, will self register itself

const unsigned char bmp_load [] = {
    0x20, 0x22, 0x04, 0x70, 0x88, 0x8B, 0x88, 0x70, 0x04, 0x22, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x07, 0x04, 0x07, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char bmp_arrow_right [] = {
    0x41, 0x63, 0x36, 0x1C
};

const unsigned char bmp_pv_panel [] = {
    0x60, 0x98, 0x86, 0xC9, 0x31, 0x19, 0x96, 0x62, 0x32, 0x2C, 0xC4, 0x64, 0x98, 0x08, 0xC8, 0x30,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x02, 0x02, 0x03, 0x04, 0x04, 0x04, 0x03, 0x00, 0x00
};

const unsigned char bmp_disconnected [] = {
    0x08, 0x08, 0x08, 0x08, 0x00, 0x41, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x41, 0x00, 0x08, 0x08,
    0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

I2C i2c(PIN_UEXT_SDA, PIN_UEXT_SCL);
Adafruit_SSD1306_I2c oled(i2c, PIN_UEXT_SSEL, 0x78, 64, 128);

UExtOled::UExtOled() {}

void UExtOled::enable() {
#ifdef PIN_UEXT_DIS
    DigitalOut uext_dis(PIN_UEXT_DIS);
    uext_dis = 0;
#endif
}

void UExtOled::process_asap() {}

void UExtOled::process_1s() {
    oled.clearDisplay();

    oled.drawBitmap(6, 0, bmp_pv_panel, 16, 16, 1);
    oled.drawBitmap(104, 0, bmp_load, 16, 16, 1);

    if (half_bridge_enabled() == false) {
        oled.drawBitmap(27, 3, bmp_disconnected, 32, 8, 1);
    }
    else {
        oled.drawBitmap(34, 3, bmp_arrow_right, 5, 7, 1);
    }

    if (load.state == LOAD_STATE_ON) {
        oled.drawBitmap(84, 3, bmp_arrow_right, 5, 7, 1);
    }
    else {
        oled.drawBitmap(81, 3, bmp_disconnected, 17, 7, 1);
    }

    oled.drawRect(52, 2, 18, 9, 1);     // battery shape
    oled.drawRect(69, 3, 3, 7, 1);      // battery terminal
    oled.drawRect(54, 4, 2, 5, 1);      // bar 1
    oled.drawRect(57, 4, 2, 5, 1);      // bar 2
    oled.drawRect(60, 4, 2, 5, 1);      // bar 3
    oled.drawRect(63, 4, 2, 5, 1);      // bar 4
    oled.drawRect(66, 4, 2, 5, 1);      // bar 5

    // solar panel data
#ifdef CHARGER_TYPE_PWM
    if (pwm_switch_enabled()) {
#else
    if (half_bridge_enabled()) {
#endif
        oled.setTextCursor(0, 18);
        oled.printf("%4.0fW", (abs(solar_terminal.power) < 1) ? 0 : -solar_terminal.power);     // remove negative zeros
    }
    else {
        oled.setTextCursor(8, 18);
        oled.printf("n/a");
    }
#ifndef CHARGER_TYPE_PWM
    if (solar_terminal.voltage > bat_terminal.voltage)
#endif
    {
        oled.setTextCursor(0, 26);
        oled.printf("%4.1fV", solar_terminal.voltage);
    }

    // battery data
    oled.setTextCursor(42, 18);
    oled.printf("%5.1fW", (abs(bat_terminal.power) < 0.1) ? 0 : bat_terminal.power);    // remove negative zeros
    oled.setTextCursor(42, 26);
    oled.printf("%5.1fV", bat_terminal.voltage);

    // load data
    oled.setTextCursor(90, 18);
    oled.printf("%5.1fW", (abs(load_terminal.power) < 0.1) ? 0 : load_terminal.power);    // remove negative zeros
    oled.setTextCursor(90, 26);
    oled.printf("%5.1fA\n", (abs(load_terminal.current) < 0.1) ? 0 : load_terminal.current);

    oled.setTextCursor(0, 36);
    oled.printf("Day +%5.0fWh -%5.0fWh", solar_terminal.neg_energy_Wh, fabs(load_terminal.pos_energy_Wh));
    oled.printf("Tot +%4.1fkWh -%4.1fkWh", dev_stat.solar_in_total_Wh / 1000.0, fabs(dev_stat.load_out_total_Wh) / 1000.0);

    oled.setTextCursor(0, 56);
#ifdef CHARGER_TYPE_PWM
    oled.printf("T %.0fC PWM %.0f%% SOC %d%%", charger.bat_temperature, pwm_switch_get_duty_cycle() * 100.0, charger.soc);
#else
    oled.printf("T %.0fC PWM %.0f%% SOC %d%%", charger.bat_temperature, half_bridge_get_duty_cycle() * 100.0, charger.soc);
#endif

    oled.display();
}

#endif /* OLED_ENABLED */

#endif /* UNIT_TEST */
