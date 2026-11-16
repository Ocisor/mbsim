// GENERATED from the CODAL sources - do not edit by hand.
// Source: lancaster-university/codal-core @ 010655d
//         lancaster-university/codal-microbit-v2 @ c849b86
// Copyright (c) 2016-2017 Lancaster University; (c) 2016 British Broadcasting
// Corporation (sound expression data). MIT licence, see LICENSE.

#ifndef MBSIM_CODAL_CONSTANTS_H
#define MBSIM_CODAL_CONSTANTS_H

enum ErrorCode{
    DEVICE_OK = 0,
    DEVICE_INVALID_PARAMETER = -1001,
    DEVICE_NOT_SUPPORTED = -1002,
    DEVICE_CALIBRATION_IN_PROGRESS = -1003,
    DEVICE_CALIBRATION_REQUIRED = -1004,
    DEVICE_NO_RESOURCES = -1005,
    DEVICE_BUSY = -1006,
    DEVICE_CANCELLED = -1007,
    DEVICE_I2C_ERROR = -1010,
    DEVICE_SERIAL_IN_USE = -1011,
    DEVICE_NO_DATA = -1012,
    DEVICE_NOT_IMPLEMENTED = -1013,
    DEVICE_SPI_ERROR = -1014,
    DEVICE_INVALID_STATE = -1015,
};

enum PanicCode{
    DEVICE_OOM = 20,
    DEVICE_RESORUCES_EXHAUSTED = 21,
    DEVICE_HEAP_ERROR = 30,
    DEVICE_NULL_DEREFERENCE = 40,
    DEVICE_PERIPHERAL_ERROR = 50,
    DEVICE_JACDAC_ERROR = 60,
    DEVICE_CPU_SDK = 70,
    DEVICE_HARDWARE_CONFIGURATION_ERROR = 90,
};

#define DEVICE_ID_BUTTON_A                                    1
#define DEVICE_ID_BUTTON_B                                    2
#define DEVICE_ID_BUTTON_AB                                   3
#define DEVICE_ID_BUTTON_RESET                                4
#define DEVICE_ID_ACCELEROMETER                               5
#define DEVICE_ID_COMPASS                                     6
#define DEVICE_ID_DISPLAY                                     7
#define DEVICE_ID_THERMOMETER                                 8
#define DEVICE_ID_RADIO                                       9
#define DEVICE_ID_RADIO_DATA_READY                            10
#define DEVICE_ID_MULTIBUTTON_ATTACH                          11
#define DEVICE_ID_SERIAL                                      12
#define DEVICE_ID_GESTURE                                     13
#define DEVICE_ID_SYSTEM_TIMER                                14
#define DEVICE_ID_SCHEDULER                                   15
#define DEVICE_ID_COMPONENT                                   16
#define DEVICE_ID_LIGHT_SENSOR                                17
#define DEVICE_ID_TOUCH_SENSOR                                18
#define DEVICE_ID_SYSTEM_DAC                                  19
#define DEVICE_ID_SYSTEM_MICROPHONE                           20
#define DEVICE_ID_SYSTEM_LEVEL_DETECTOR                       21
#define DEVICE_ID_SYSTEM_LEVEL_DETECTOR_SPL                   22
#define DEVICE_ID_MSC                                         23
#define DEVICE_ID_SPI                                         24
#define DEVICE_ID_DISTANCE                                    25
#define DEVICE_ID_GYROSCOPE                                   26
#define DEVICE_ID_HUMIDITY                                    27
#define DEVICE_ID_PRESSURE                                    28
#define DEVICE_ID_SINGLE_WIRE_SERIAL                          29
#define DEVICE_ID_JACDAC                                      30
#define DEVICE_ID_JACDAC_PHYS                                 31
#define DEVICE_ID_JACDAC_CONTROL_SERVICE                      32
#define DEVICE_ID_JACDAC_CONFIGURATION_SERVICE                33
#define DEVICE_ID_SYSTEM_ADC                                  34
#define DEVICE_ID_PULSE_IN                                    35
#define DEVICE_ID_USB                                         36
#define DEVICE_ID_SPLITTER                                    37
#define DEVICE_ID_AUDIO_PROCESSOR                             38
#define DEVICE_ID_TAP                                         39
#define DEVICE_ID_POWER_MANAGER                               40
#define DEVICE_ID_PARTIAL_FLASHING                            41
#define DEVICE_ID_USB_FLASH_MANAGER                           42
#define DEVICE_ID_VIRTUAL_SPEAKER_PIN                         43
#define DEVICE_ID_LOG                                         44
#define DEVICE_ID_IO_P0                                       100
#define DEVICE_ID_MESSAGE_BUS_IGNORED                         1020
#define DEVICE_ID_MESSAGE_BUS_LISTENER                        1021
#define DEVICE_ID_NOTIFY_ONE                                  1022
#define DEVICE_ID_NOTIFY                                      1023
#define DEVICE_ID_BUTTON_UP                                   2000
#define DEVICE_ID_BUTTON_DOWN                                 2001
#define DEVICE_ID_BUTTON_LEFT                                 2002
#define DEVICE_ID_BUTTON_RIGHT                                2003
#define DEVICE_ID_JD_DYNAMIC_ID                               3000
#define DEVICE_ID_DYNAMIC_MIN                                 64000
#define DEVICE_ID_DYNAMIC_MAX                                 65000
#define DEVICE_COMPONENT_RUNNING                              0x1000
#define DEVICE_COMPONENT_STATUS_SYSTEM_TICK                   0x2000
#define DEVICE_COMPONENT_STATUS_IDLE_TICK                     0x4000
#define DEVICE_COMPONENT_LISTENERS_CONFIGURED                 0x01
#define DEVICE_COMPONENT_EVT_SYSTEM_TICK                      1
#define DEVICE_ID_ANY                                         0
#define DEVICE_EVT_ANY                                        0
#define DEVICE_BUTTON_EVT_DOWN                                1
#define DEVICE_BUTTON_EVT_UP                                  2
#define DEVICE_BUTTON_EVT_CLICK                               3
#define DEVICE_BUTTON_EVT_LONG_CLICK                          4
#define DEVICE_BUTTON_EVT_HOLD                                5
#define DEVICE_BUTTON_EVT_DOUBLE_CLICK                        6
#define DEVICE_BUTTON_LONG_CLICK_TIME                         1000
#define DEVICE_BUTTON_HOLD_TIME                               1500
#define DEVICE_BUTTON_STATE                                   0x01
#define DEVICE_BUTTON_STATE_HOLD_TRIGGERED                    0x02
#define DEVICE_BUTTON_STATE_CLICK                             0x04
#define DEVICE_BUTTON_STATE_LONG_CLICK                        0x08
#define DEVICE_BUTTON_SIGMA_MIN                               0
#define DEVICE_BUTTON_SIGMA_MAX                               12
#define DEVICE_BUTTON_SIGMA_THRESH_HI                         8
#define DEVICE_BUTTON_SIGMA_THRESH_LO                         2
#define DEVICE_BUTTON_DOUBLE_CLICK_THRESH                     50
#define DISPLAY_EVT_FREE                                      1
#define CODAL_SERIAL_EVT_TX_EMPTY                             2
#define BLE_EVT_SERIAL_TX_EMPTY                               3
#define ARCADE_PLAYER_JOIN_RESULT                             4
#define POWER_EVT_CANCEL_DEEPSLEEP                            5
#define DEVICE_NOTIFY_USER_EVENT_BASE                         1024
#define MESSAGE_BUS_LISTENER_PARAMETERISED                    0x0001
#define MESSAGE_BUS_LISTENER_METHOD                           0x0002
#define MESSAGE_BUS_LISTENER_BUSY                             0x0004
#define MESSAGE_BUS_LISTENER_REENTRANT                        0x0008
#define MESSAGE_BUS_LISTENER_QUEUE_IF_BUSY                    0x0010
#define MESSAGE_BUS_LISTENER_DROP_IF_BUSY                     0x0020
#define MESSAGE_BUS_LISTENER_NONBLOCKING                      0x0040
#define MESSAGE_BUS_LISTENER_URGENT                           0x0080
#define MESSAGE_BUS_LISTENER_DELETING                         0x8000
#define MESSAGE_BUS_LISTENER_IMMEDIATE                        (MESSAGE_BUS_LISTENER_NONBLOCKING |  MESSAGE_BUS_LISTENER_URGENT)
#define DISPLAY_EVT_ANIMATION_COMPLETE                        1
#define DISPLAY_DEFAULT_AUTOCLEAR                             1
#define DISPLAY_SPACING                                       1
#define DISPLAY_ANIMATE_DEFAULT_POS                           -255
#define DISPLAY_DEFAULT_SCROLL_SPEED                          120
#define DISPLAY_DEFAULT_SCROLL_STRIDE                         -1
#define DISPLAY_DEFAULT_PRINT_SPEED                           400
#define DEVICE_SCHEDULER_RUNNING                              0x01
#define DEVICE_SCHEDULER_IDLE                                 0x02
#define DEVICE_SCHEDULER_DEEPSLEEP                            0x04
#define DEVICE_FIBER_FLAG_FOB                                 0x01
#define DEVICE_FIBER_FLAG_PARENT                              0x02
#define DEVICE_FIBER_FLAG_CHILD                               0x04
#define DEVICE_FIBER_FLAG_DO_NOT_PAGE                         0x08
#define DEVICE_SCHEDULER_EVT_TICK                             1
#define DEVICE_SCHEDULER_EVT_IDLE                             2
#define MULTI_BUTTON_STATE_1                                  0x01
#define MULTI_BUTTON_STATE_2                                  0x02
#define MULTI_BUTTON_HOLD_TRIGGERED_1                         0x04
#define MULTI_BUTTON_HOLD_TRIGGERED_2                         0x08
#define MULTI_BUTTON_SUPRESSED_1                              0X10
#define MULTI_BUTTON_SUPRESSED_2                              0x20
#define MULTI_BUTTON_ATTACHED                                 0x40
#define BITMAP_FONT_WIDTH                                     5
#define BITMAP_FONT_HEIGHT                                    5
#define BITMAP_FONT_ASCII_START                               32
#define BITMAP_FONT_ASCII_END                                 126
#define DEVICE_ID_SOUND_EMOJI_SYNTHESIZER_0                   3010
#define DEVICE_ID_SOUND_EMOJI_SYNTHESIZER_1                   3011
#define DEVICE_ID_SOUND_EMOJI_SYNTHESIZER_2                   3012
#define DEVICE_ID_SOUND_EMOJI_SYNTHESIZER_3                   3013
#define DEVICE_ID_SOUND_EMOJI_SYNTHESIZER_4                   3014
#define DEVICE_ID_SOUND_EMOJI_SYNTHESIZER_5                   3015
#define DEVICE_ID_SOUND_EMOJI_SYNTHESIZER_6                   3016
#define DEVICE_ID_SOUND_EMOJI_SYNTHESIZER_7                   3017
#define DEVICE_ID_SOUND_EMOJI_SYNTHESIZER_8                   3018
#define DEVICE_ID_SOUND_EMOJI_SYNTHESIZER_9                   3019
#define DEVICE_SOUND_EMOJI_SYNTHESIZER_EVT_DONE               1
#define DEVICE_SOUND_EMOJI_SYNTHESIZER_EVT_PLAYBACK_COMPLETE  2
#define ID_PIN_P0                                             (DEVICE_ID_IO_P0 + 0)
#define ID_PIN_P1                                             (DEVICE_ID_IO_P0 + 1)
#define ID_PIN_P2                                             (DEVICE_ID_IO_P0 + 2)
#define ID_PIN_P3                                             (DEVICE_ID_IO_P0 + 3)
#define ID_PIN_P4                                             (DEVICE_ID_IO_P0 + 4)
#define ID_PIN_P5                                             (DEVICE_ID_IO_P0 + 5)
#define ID_PIN_P6                                             (DEVICE_ID_IO_P0 + 6)
#define ID_PIN_P7                                             (DEVICE_ID_IO_P0 + 7)
#define ID_PIN_P8                                             (DEVICE_ID_IO_P0 + 8)
#define ID_PIN_P9                                             (DEVICE_ID_IO_P0 + 9)
#define ID_PIN_P10                                            (DEVICE_ID_IO_P0 + 10)
#define ID_PIN_P11                                            (DEVICE_ID_IO_P0 + 11)
#define ID_PIN_P12                                            (DEVICE_ID_IO_P0 + 12)
#define ID_PIN_P13                                            (DEVICE_ID_IO_P0 + 13)
#define ID_PIN_P14                                            (DEVICE_ID_IO_P0 + 14)
#define ID_PIN_P15                                            (DEVICE_ID_IO_P0 + 15)
#define ID_PIN_P16                                            (DEVICE_ID_IO_P0 + 16)
#define ID_PIN_P17                                            (DEVICE_ID_IO_P0 + 17)
#define ID_PIN_P18                                            (DEVICE_ID_IO_P0 + 18)
#define ID_PIN_P19                                            (DEVICE_ID_IO_P0 + 19)
#define ID_PIN_P20                                            (DEVICE_ID_IO_P0 + 20)
#define ID_PIN_LOGO                                           (DEVICE_ID_IO_P0 + 21)
#define ID_PIN_SPEAKER                                        (DEVICE_ID_IO_P0 + 22)
#define ID_PIN_RUNMIC                                         (DEVICE_ID_IO_P0 + 23)
#define ID_PIN_SDA                                            (DEVICE_ID_IO_P0 + 24)
#define ID_PIN_SCL                                            (DEVICE_ID_IO_P0 + 25)
#define ID_PIN_ROW1                                           (DEVICE_ID_IO_P0 + 26)
#define ID_PIN_ROW2                                           (DEVICE_ID_IO_P0 + 27)
#define ID_PIN_ROW3                                           (DEVICE_ID_IO_P0 + 28)
#define ID_PIN_ROW4                                           (DEVICE_ID_IO_P0 + 29)
#define ID_PIN_ROW5                                           (DEVICE_ID_IO_P0 + 30)
#define ID_PIN_USBTX                                          (DEVICE_ID_IO_P0 + 31)
#define ID_PIN_USBRX                                          (DEVICE_ID_IO_P0 + 32)
#define ID_PIN_IRQ1                                           (DEVICE_ID_IO_P0 + 33)
#define ID_PIN_MIC                                            (DEVICE_ID_IO_P0 + 34)
#define ID_PIN_P35                                            (DEVICE_ID_IO_P0 + 35)
#define ID_PIN_P36                                            (DEVICE_ID_IO_P0 + 36)
#define ID_PIN_P37                                            (DEVICE_ID_IO_P0 + 37)
#define ID_PIN_P38                                            (DEVICE_ID_IO_P0 + 38)
#define ID_PIN_P39                                            (DEVICE_ID_IO_P0 + 39)
#define ID_PIN_P40                                            (DEVICE_ID_IO_P0 + 40)
#define ID_PIN_P41                                            (DEVICE_ID_IO_P0 + 41)
#define ID_PIN_P42                                            (DEVICE_ID_IO_P0 + 42)
#define ID_PIN_P43                                            (DEVICE_ID_IO_P0 + 43)
#define ID_PIN_P44                                            (DEVICE_ID_IO_P0 + 44)
#define ID_PIN_P45                                            (DEVICE_ID_IO_P0 + 45)
#define ID_PIN_P46                                            (DEVICE_ID_IO_P0 + 46)
#define ID_PIN_P47                                            (DEVICE_ID_IO_P0 + 47)
#define SCHEDULER_TICK_PERIOD_US                              4000
#define MESSAGE_BUS_LISTENER_MAX_QUEUE_DEPTH                  100
#define EVENT_LISTENER_DEFAULT_FLAGS                          MESSAGE_BUS_LISTENER_QUEUE_IF_BUSY

// MICROBIT_* aliases, as in codal-microbit-v2/inc/compat/MicroBitCompat.h
#define MICROBIT_BUSY                            DEVICE_BUSY
#define MICROBIT_CANCELLED                       DEVICE_CANCELLED
#define MICROBIT_CALIBRATION_IN_PROGRESS         DEVICE_CALIBRATION_IN_PROGRESS
#define MICROBIT_CALIBRATION_REQUIRED            DEVICE_CALIBRATION_REQUIRED
#define MICROBIT_HEAP_ERROR                      DEVICE_HEAP_ERROR
#define MICROBIT_I2C_ERROR                       DEVICE_I2C_ERROR
#define MICROBIT_INVALID_PARAMETER               DEVICE_INVALID_PARAMETER
#define MICROBIT_NO_DATA                         DEVICE_NO_DATA
#define MICROBIT_NO_RESOURCES                    DEVICE_NO_RESOURCES
#define MICROBIT_NOT_SUPPORTED                   DEVICE_NOT_SUPPORTED
#define MICROBIT_NULL_DEREFERENCE                DEVICE_NULL_DEREFERENCE
#define MICROBIT_OK                              DEVICE_OK
#define MICROBIT_OOM                             DEVICE_OOM
#define MICROBIT_SERIAL_IN_USE                   DEVICE_SERIAL_IN_USE
#define MICROBIT_COMPONENT_RUNNING               DEVICE_COMPONENT_RUNNING
#define MICROBIT_DEFAULT_PRINT_SPEED             DISPLAY_DEFAULT_PRINT_SPEED
#define MICROBIT_DEFAULT_SCROLL_SPEED            DISPLAY_DEFAULT_SCROLL_SPEED
#define MICROBIT_DEFAULT_SCROLL_STRIDE           DISPLAY_DEFAULT_SCROLL_STRIDE
#define MICROBIT_DISPLAY_DEFAULT_AUTOCLEAR       DISPLAY_DEFAULT_AUTOCLEAR
#define MICROBIT_DISPLAY_ANIMATE_DEFAULT_POS     DISPLAY_ANIMATE_DEFAULT_POS
#define MICROBIT_FONT_ASCII_END                  BITMAP_FONT_ASCII_END
#define MICROBIT_FONT_ASCII_START                BITMAP_FONT_ASCII_START
#define MICROBIT_FONT_HEIGHT                     BITMAP_FONT_HEIGHT
#define MICROBIT_FONT_WIDTH                      BITMAP_FONT_WIDTH
#define MICROBIT_UART_S_EVT_TX_EMPTY             BLE_EVT_SERIAL_TX_EMPTY
#define MICROBIT_BUTTON_ALL_EVENTS               DEVICE_BUTTON_ALL_EVENTS
#define MICROBIT_BUTTON_SIMPLE_EVENTS            DEVICE_BUTTON_SIMPLE_EVENTS
#define MICROBIT_BUTTON_EVT_CLICK                DEVICE_BUTTON_EVT_CLICK
#define MICROBIT_BUTTON_EVT_DOUBLE_CLICK         DEVICE_BUTTON_EVT_DOUBLE_CLICK
#define MICROBIT_BUTTON_EVT_DOWN                 DEVICE_BUTTON_EVT_DOWN
#define MICROBIT_BUTTON_EVT_HOLD                 DEVICE_BUTTON_EVT_HOLD
#define MICROBIT_BUTTON_EVT_LONG_CLICK           DEVICE_BUTTON_EVT_LONG_CLICK
#define MICROBIT_BUTTON_EVT_UP                   DEVICE_BUTTON_EVT_UP
#define MICROBIT_DISPLAY_EVT_ANIMATION_COMPLETE  DISPLAY_EVT_ANIMATION_COMPLETE
#define MICROBIT_DISPLAY_EVT_FREE                DISPLAY_EVT_FREE
#define MICROBIT_SERIAL_EVT_TX_EMPTY             CODAL_SERIAL_EVT_TX_EMPTY
#define MICROBIT_ID_ANY                          DEVICE_ID_ANY
#define MICROBIT_EVT_ANY                         DEVICE_EVT_ANY
#define MICROBIT_ID_ACCELEROMETER                DEVICE_ID_ACCELEROMETER
#define MICROBIT_ID_BUTTON_A                     DEVICE_ID_BUTTON_A
#define MICROBIT_ID_BUTTON_B                     DEVICE_ID_BUTTON_B
#define MICROBIT_ID_BUTTON_AB                    DEVICE_ID_BUTTON_AB
#define MICROBIT_ID_BUTTON_RESET                 DEVICE_ID_BUTTON_RESET
#define MICROBIT_ID_COMPASS                      DEVICE_ID_COMPASS
#define MICROBIT_ID_DISPLAY                      DEVICE_ID_DISPLAY
#define MICROBIT_ID_GESTURE                      DEVICE_ID_GESTURE
#define MICROBIT_ID_IO_P0                        ID_PIN_P0
#define MICROBIT_ID_IO_P1                        ID_PIN_P1
#define MICROBIT_ID_IO_P2                        ID_PIN_P2
#define MICROBIT_ID_IO_P3                        ID_PIN_P3
#define MICROBIT_ID_IO_P4                        ID_PIN_P4
#define MICROBIT_ID_IO_P5                        ID_PIN_P5
#define MICROBIT_ID_IO_P6                        ID_PIN_P6
#define MICROBIT_ID_IO_P7                        ID_PIN_P7
#define MICROBIT_ID_IO_P8                        ID_PIN_P8
#define MICROBIT_ID_IO_P9                        ID_PIN_P9
#define MICROBIT_ID_IO_P10                       ID_PIN_P10
#define MICROBIT_ID_IO_P11                       ID_PIN_P11
#define MICROBIT_ID_IO_P12                       ID_PIN_P12
#define MICROBIT_ID_IO_P13                       ID_PIN_P13
#define MICROBIT_ID_IO_P14                       ID_PIN_P14
#define MICROBIT_ID_IO_P15                       ID_PIN_P15
#define MICROBIT_ID_IO_P16                       ID_PIN_P16
#define MICROBIT_ID_IO_P19                       ID_PIN_P19
#define MICROBIT_ID_IO_P20                       ID_PIN_P20
#define MICROBIT_ID_LOGO                         ID_PIN_LOGO
#define MICROBIT_ID_MESSAGE_BUS_LISTENER         DEVICE_ID_MESSAGE_BUS_LISTENER
#define MICROBIT_ID_MULTIBUTTON_ATTACH           DEVICE_ID_MULTIBUTTON_ATTACH
#define MICROBIT_ID_NOTIFY                       DEVICE_ID_NOTIFY
#define MICROBIT_ID_NOTIFY_ONE                   DEVICE_ID_NOTIFY_ONE
#define MICROBIT_ID_RADIO                        DEVICE_ID_RADIO
#define MICROBIT_ID_RADIO_DATA_READY             DEVICE_ID_RADIO_DATA_READY
#define MICROBIT_ID_SERIAL                       DEVICE_ID_SERIAL
#define MICROBIT_ID_THERMOMETER                  DEVICE_ID_THERMOMETER
#define MICROBIT_ID_POWER_MANAGER                DEVICE_ID_POWER_MANAGER
#define MICROBIT_ID_PARTIAL_FLASHING             DEVICE_ID_PARTIAL_FLASHING
#define MICROBIT_ID_USB_FLASH_MANAGER            DEVICE_ID_USB_FLASH_MANAGER
#define MICROBIT_ID_VIRTUAL_SPEAKER_PIN          DEVICE_ID_VIRTUAL_SPEAKER_PIN
#define MICROBIT_ID_LOG                          DEVICE_ID_LOG
#define MICROBIT_SCHEDULER_RUNNING               DEVICE_SCHEDULER_RUNNING

#endif
