## Table of Contents
- [Introduction](#intro)
- [Version](#ver)
- [Integration details](#integration)
- [Driver files information](#fileinfo)
- [Supported sensor interface](#interface)
- [Copyright](#copy)

### Introduction<a name=intro></a>
- This package contains the Bosch Sensortec MEMS gyroscope sensor driver (sensor API)
- The sensor driver package includes bmg160.h, bmg160.c and bmg160_support.c files
- BMG160 sensor driver supports the following sensors
    * BMG160
    * BMI055 - combination of bma2x2 + bmg160
    * BMX055 - combination of bma2x2 + bmg160 + bmm050

### Version<a name=ver></a>
- Version of bmg160 sensor driver is:
    * bmg160.c 		- V2.0.11
    * bmg160.h 		- V2.0.11
    * bmg160_support.c 	- V2.0.11

### Integration details<a name=integration></a>
- Integrate bmg160.h and bmg160.c file in to your project.
- The bmg160_support.c file contains only examples for API use cases, so it is not required to integrate into project.

### Driver files information<a name=fileinfo></a>
- bmg160.h
    * This header file has the register address definition, constant definitions, data type definition and supported sensor driver calls declarations.
- bmg160.c
    * This file contains the implementation for the sensor driver APIs.
- bmg160_support.c
    * This file shall be used as an user guidance, here you can find samples of
            * Initialize the sensor with I2C/SPI communication
                    - Add your code to the SPI and/or I2C bus read and bus write functions.
                            - Return value can be chosen by yourself
                        - API just passes that value to your application code
                    - Add your code to the delay function
                    - Change I2C address accordingly in bmg160.h
        * Power mode configuration of the sensor
        * Get and set functions usage
        * Reading the sensor read out data

### Supported sensor interface<a name=interface></a>
- This gyroscope sensor driver supports SPI and I2C interfaces

### Copyright<a name=copy></a>
- Copyright (C) 2018 - 2019 Bosch Sensortec GmbH

