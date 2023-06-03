# UISO

> Originally called MISO IoT project. Pronounced: mUISO 

# The name

This project was to be called `miso` as it has a dual meaning:

- [Tasty japanese seasoning](https://en.wikipedia.org/wiki/Miso)
- [Master-In-Slave-Out](https://en.wikipedia.org/wiki/Serial_Peripheral_Interface)

Since *every* embedded developer will encounter serial peripheral interfaces at some point in their careers, this makes a catchy choice. 

However the there are some software projects that already use the name, so I substituded the m for a faux *micro* (u), hence the name `uiso`

# The System

`uiso` is based on the hardware of the Bosch XDK110 embedded sensor. 

The software however follows another stack and architecture. 

## Core Stack

This project attempts to maximize the use of library functions provided by respectful and established sources. 
Great care has been taken to ensure simplicity and the integration of 3rd party libraries.

- Micro Controller and Hardware Abstraction Layers
    - Sillicon Labs [Gecko SDK](https://github.com/SiliconLabs/gecko_sdk)
      - emlib
      - emdrv
        - dmadrv
        - gpiointerrupt
        - spidrv
        - tempdrv
      - driver
        - button
        - debug
        - i2cspm
        - leddrv
      - service
        - device_init
        - iostream
        - power_manager
        - sleeptimer
        - udelay
- Device Drivers
    - [Texas Instruments SimpleLink CC3100 SDK]()
    - [Bosch Sensortec BMA2x]()
    - [Bosch Sensortec BMG160]()
    - [Bosch Sensortec BME280]()
    - [Bosch Sensortec BMI160]()
    - [Bosch Sensortec BMM150]().
- RTOS
    - [FreeRTOS](https://www.freertos.org/).
- Services
    - [mbedTLS](https://github.com/Mbed-TLS/mbedtls). TLS and Crypto Layer Support.
    - [jsmn](https://github.com/zserge/jsmn) JSON parser.
    - [wakaama](https://www.eclipse.org/wakaama/)	LWM2M service
    - [FatFS 15](http://elm-chan.org/fsw/ff/00index_e.html)
 
 Future:
 - SipHash
 - Paho Embedded
 - sqlite

## Core Features

Uiso is able to read configuration files, to connect to a designated WiFi AP, to get a timestamp from a internet server and to provide a basic LWM2M service with connection management.

- FreeRTOS Runtime
- LEDs (Red, Orange, Yellow)
- Buttons 1 and 2 plus interrupts
- RTC and sleeptimer
- RTC Calender
- SPI driver for Simplelink and SD Card
- SD Card driver
- FatFS adaption (including RTC driver)
- Wifi Service
- sNTP timestamp synchronization 
- Wakaama LWM2M service with Device, Temperature and Accelerometer objects
- mbedTLS PSK and x509 certificate authentication tested with DTLS
- Sensors (Bosch Sensortec)
- I2C Master
- Basic multi-protocol support.

## Non-Features

For design, and legal reasons, following hardware capabilities of the XDK110 will not be supported:

- BLE
- Light Sensor
- Accoustic sensor / Microphone AKU340


# Disclaimer

- This project is not affiliated with [LEGIC Identsystems Ltd](https://www.legic.com/)
- This project is not affiliated with the [LEGIC XDK Secure Sensor Evaluation Kit](https://www.xdk.io/) which uses the same hardware. 
- This project is not affiliated with the Rust Foundation or the Rust Project.
- This project is a non-commercial project 

# Licensing

Application software is to be licensed under the MIT License. Other software (in binary form or source form) may have a different license. Please refer to the corresponding file headers and license files.
In case a source module has been derived from, or uses significant code contributions from other licensed software, then the original license will be applied. 

Propper licensing information and software BOM will be provided in the future. 

# IMPORTANT 
> Do not use this codebase in use cases which have safety and/or availability requirements.
