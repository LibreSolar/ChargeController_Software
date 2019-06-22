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

#ifndef MPPT_2420_LC_0V10_H
#define MPPT_2420_LC_0V10_H

#define DEVICE_TYPE "MPPT-2420-LC"
#define HARDWARE_VERSION "v0.10"

#include "mbed.h"

// DC/DC converter settings
#define PWM_FREQUENCY 70 // kHz  70 = good compromise between output ripple and efficiency
#define PWM_TIM        1 // use TIM1 timer

#define DCDC_CURRENT_MAX 20  // PCB maximum DCDC output current
#define LOAD_CURRENT_MAX 20  // PCB maximum load switch current

#define PIN_UEXT_DIS  PC_14     // starting from rev. 0.10
#define PIN_UEXT_TX   PA_2
#define PIN_UEXT_RX   PA_3
#define PIN_UEXT_SCL  PB_6
#define PIN_UEXT_SDA  PB_7
#define PIN_UEXT_MISO PB_4
#define PIN_UEXT_MOSI PB_5
#define PIN_UEXT_SCK  PB_3
#define PIN_UEXT_SSEL PC_13     // PCB rev 0.6: PA_0

#define PIN_SWD_TX    PA_9
#define PIN_SWD_RX    PA_10
/*
#define PIN_LED_PWR   PB_14
#define PIN_LED_LOAD  PB_15
*/
#define PIN_LOAD_DIS    PB_2
//#define PIN_5V_OUT_EN PB_12
#define PIN_USB_PWR_EN  PB_12       // normally should be named 5V_OUT_EN, as no USB is existing here
//#define PIN_USB_PWR_FLG PC_14
#define PIN_CAN_RX    PB_8
#define PIN_CAN_TX    PB_9
#define PIN_CAN_STB   PA_15
#define PIN_V_BUS_DIS PC_15     // bus power supply disable, starting from rev. 0.10

#define PIN_REF_I_DCDC PA_4

#define PIN_EEPROM_SCL PB_10
#define PIN_EEPROM_SDA PB_11

#define EEPROM_24AA32

enum pin_state_t { PIN_HIGH, PIN_LOW, PIN_FLOAT };

// assignment LED numbers on PCB to their meaning
#define NUM_LEDS 2

#define LED_PWR  0     // LED1
#define LED_LOAD 1     // LED2

// LED pins and pin state configuration to switch above LEDs on
#define NUM_LED_PINS 2
static const PinName led_pins[NUM_LED_PINS] = {
    //  PWR      LOAD
       PB_14,   PB_15
};
static const pin_state_t led_pin_setup[NUM_LEDS][NUM_LED_PINS] = {
    { PIN_HIGH, PIN_LOW  }, // LED1
    { PIN_LOW,  PIN_HIGH }  // LED2
};

// pin definition only needed in adc_dma.cpp to detect if they are present on the PCB
#define PIN_ADC_TEMP_BAT    PA_0
#define PIN_ADC_TEMP_FETS   PA_1

// typical value for Semitec 103AT-5 thermistor: 3435
#define NTC_BETA_VALUE 3435
#define NTC_SERIES_RESISTOR 10000.0

#define ADC_GAIN_V_BAT (110.0 / 10 )    // battery voltage divider 100k + 10k
#define ADC_GAIN_V_SOLAR (105.6 / 5.6)  // solar voltage divider: 100k + 5.6k
#define ADC_GAIN_I_LOAD (1000 / 2 / (1500.0 / 22.0)) // op amp gain: 150/2.2 = 68.2, resistor: 2 mOhm
#define ADC_GAIN_I_DCDC (1000 / 2 / (1500.0 / 22.0))

// position in the array written by the DMA controller
enum {
    ADC_POS_TEMP_BAT,   // ADC 0 (PA_0)
    ADC_POS_TEMP_FETS,  // ADC 1 (PA_1)
    ADC_POS_V_REF,      // ADC 5 (PA_5)
    ADC_POS_V_BAT,      // ADC 6 (PA_6)
    ADC_POS_V_SOLAR,    // ADC 7 (PA_7)
    ADC_POS_I_LOAD,     // ADC 8 (PB_0)
    ADC_POS_I_DCDC,     // ADC 9 (PB_1)
    ADC_POS_TEMP_MCU,   // ADC 16
    ADC_POS_VREF_MCU,   // ADC 17
    NUM_ADC_CH          // trick to get the number of enums
};

// selected ADC channels (has to match with above enum)
#define ADC_CHSEL ( \
    ADC_CHSELR_CHSEL0 | \
    ADC_CHSELR_CHSEL1 | \
    ADC_CHSELR_CHSEL5 | \
    ADC_CHSELR_CHSEL6 | \
    ADC_CHSELR_CHSEL7 | \
    ADC_CHSELR_CHSEL8 | \
    ADC_CHSELR_CHSEL9 | \
    ADC_CHSELR_CHSEL16 | \
    ADC_CHSELR_CHSEL17 \
)

// defines for CAN BUS SUPPORT
#define HAS_CAN 1
#define PIN_CAN_RX    PB_8
#define PIN_CAN_TX    PB_9
#define PIN_CAN_STB PA_15

#endif
