/* LibreSolar charge controller firmware
 * Copyright (c) 2016-2019 Martin Jäger (www.libre.solar)
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

/** @file
 *
 * @brief Entry point of charge controller firmware
 */

#include "mbed.h"

#include "thingset.h"           // handles access to internal data via communication interfaces
#include "pcb.h"                // hardware-specific settings
#include "config.h"             // user-specific configuration

#include "half_bridge.h"        // PWM generation for DC/DC converter
#include "hardware.h"           // hardware-related functions like load switch, LED control, watchdog, etc.
#include "dcdc.h"               // DC/DC converter control (hardware independent)
#include "pwm_switch.h"         // PWM charge controller
#include "bat_charger.h"        // battery settings and charger state machine
#include "adc_dma.h"            // ADC using DMA and conversion to measurement values
#include "uext.h"               // communication interfaces, displays, etc. in UEXT connector
#include "eeprom.h"             // external I2C EEPROM
#include "load.h"               // load and USB output management
#include "leds.h"               // LED switching using charlieplexing
#include "log.h"                // log data (error memory, min/max measurements, etc.)
#include "data_objects.h"       // for access to internal data via ThingSet
#include "thingset_serial.h"    // UART or USB serial communication
#include "thingset_can.h"       // CAN bus communication

DcBus lv_bus;
PowerPort lv_terminal(&lv_bus);         // low voltage terminal (battery for typical MPPT)

#if FEATURE_DCDC_CONVERTER
DcBus hv_bus;
PowerPort hv_terminal(&hv_bus);         // high voltage terminal (solar for typical MPPT)
PowerPort dcdc_port_hv(&hv_bus);        // internal high voltage side port of DC/DC converter
PowerPort dcdc_port_lv(&lv_bus);        // internal low voltage side of DC/DC converter
Dcdc dcdc(&dcdc_port_hv, &dcdc_port_lv, DCDC_MODE_INIT);
#endif

#if FEATURE_PWM_SWITCH
DcBus hv_bus;
PowerPort hv_terminal(&hv_bus);         // high voltage terminal (solar for typical MPPT)
PwmSwitch pwm_switch = {};
#endif

#if FEATURE_LOAD_OUTPUT
PowerPort load_terminal(&lv_bus);       // load terminal (also connected to lv_bus)
LoadOutput load(&load_terminal);
#endif

// configure actual terminal connections as defined in config.h
PowerPort &bat_terminal = BATTERY_TERMINAL;
PowerPort &solar_terminal = SOLAR_TERMINAL;
#ifdef GRID_TERMINAL
PowerPort &grid_terminal = GRID_TERMINAL;
#endif

Charger charger(&bat_terminal);

BatConf bat_conf;               // actual (used) battery configuration
BatConf bat_conf_user;          // temporary storage where the user can write to

LogData log_data;

extern ThingSet ts;             // defined in data_objects.cpp

time_t timestamp;    // current unix timestamp (independent of time(NULL), as it is user-configurable)

Serial serial(PIN_SWD_TX, PIN_SWD_RX, "serial", 115200);

/** High priority function for DC/DC control and safety functions
 *
 * Called by control timer with 10 Hz frequency (see hardware.cpp).
 */
void system_control()
{
    static int counter = 0;

    // convert ADC readings to meaningful measurement values
    update_measurements();

    #if FEATURE_PWM_SWITCH
    pwm_switch_control(&pwm_switch, &hv_terminal, &bat_terminal);
    leds_set_charging(pwm_switch_enabled());
    #endif

    #if FEATURE_DCDC_CONVERTER
    dcdc.control();     // control of DC/DC including MPPT algorithm
    leds_set_charging(half_bridge_enabled());
    #endif

    load.control(bat_conf.voltage_absolute_max * charger.num_batteries);

    if (counter % CONTROL_FREQUENCY == 0) {
        // called once per second (this timer is much more accurate than time(NULL) based on LSI)
        // see also here: https://github.com/ARMmbed/mbed-os/issues/9065
        timestamp++;
        counter = 0;
        // energy + soc calculation must be called exactly once per second
        solar_terminal.energy_balance();
        bat_terminal.energy_balance();
        load_terminal.energy_balance();
        log_update_energy(&log_data);
        log_update_min_max_values(&log_data);
        charger.update_soc(&bat_conf);
    }
    counter++;
}

/** Main function including initialization and continuous loop
 */
int main()
{
    leds_init();

    battery_conf_init(&bat_conf, BATTERY_TYPE, BATTERY_NUM_CELLS, BATTERY_CAPACITY);
    battery_conf_overwrite(&bat_conf, &bat_conf_user);  // initialize conf_user with same values

    #if FEATURE_DCDC_CONVERTER
    // lower duty limit might have to be adjusted dynamically depending on LS voltage
    half_bridge_init(PWM_FREQUENCY, PWM_DEADTIME, 12 / dcdc.hs_voltage_max, 0.97);
    #endif

    #if FEATURE_PWM_SWITCH
    pwm_switch_init(&pwm_switch);
    #endif

    // Configuration from EEPROM
    data_objects_read_eeprom();
    ts.set_conf_callback(data_objects_update_conf);     // after each configuration change, data should be written back to EEPROM
    ts.set_user_password(THINGSET_USER_PASSWORD);       // passwords defined in config.h (see template)
    ts.set_maker_password(THINGSET_MAKER_PASSWORD);

    // ADC, DMA and sensor calibration
    adc_setup();
    dma_setup();
    adc_timer_start(1000);  // 1 kHz
    wait(0.5);      // wait for ADC to collect some measurement values
    update_measurements();
    calibrate_current_sensors();

    // Communication interfaces
    ts_interfaces.enable();

    uext_init();
    init_watchdog(10);      // 10s should be enough for communication ports

    solar_terminal.init_solar();

    #ifdef GRID_TERMINAL
    grid_terminal.init_nanogrid();
    #endif

    charger.detect_num_batteries(&bat_conf);     // check if we have 24V instead of 12V system
    battery_init_dc_bus(&bat_terminal, &bat_conf, charger.num_batteries);

    wait(2);    // safety feature: be able to re-flash before starting
    control_timer_start(CONTROL_FREQUENCY);
    wait(0.1);  // necessary to prevent MCU from randomly getting stuck here if PV panel is connected before battery


    sleep_manager_lock_deep_sleep(); // required to have sleep returning.
    /*
        The mBed Serial class calls this internal during "attach", this is why it work with ThingSet Serial enabled even
        without this statement. Might be an issue of the particular STM32F072 mBed code or may affect
        also STM32L073 platforms on mBed
    */

    // the main loop is suitable for slow tasks like communication (even blocking wait allowed)
    time_t last_call = timestamp;
    while (1) {

        ts_interfaces.process_asap();
        uext_process_asap();

        time_t now = timestamp;
        if (now >= last_call + 1 || now < last_call) {   // called once per second (or slower if blocking wait occured somewhere)

            //printf("Still alive... time: %d, mode: %d\n", (int)time(NULL), dcdc.mode);

            charger.discharge_control(&bat_conf);
            charger.charge_control(&bat_conf);

            #if FEATURE_DCDC_CONVERTER
            update_dcdc_current_targets(&dcdc_port_lv, &bat_terminal, &load_terminal);
            #endif

            load.state_machine();

            eeprom_update();

            leds_update_1s();
            leds_update_soc(charger.soc, load.switch_state == LOAD_STATE_OFF_LOW_SOC);

            uext_process_1s();
            ts_interfaces.process_1s();

            last_call = now;
        }
        feed_the_dog();
        sleep();    // wake-up by timer interrupts
    }
}

#endif
