# Welcome to UISO

Hello and welcome! UISO is an innovative IoT project leveraging the Bosch XDK110 embedded sensor hardware.

## The Story Behind the Name

Originally, the project was intended to be named "miso", a moniker with dual significance. "Miso" is not only a delicious Japanese seasoning that everybody loves, but it also stands for "Master-In-Slave-Out", a term that every embedded developer will undoubtedly come across at some point in their career.

However, as the name 'miso' had already been taken by some other software projects, we had to make a slight modification. To keep the essence of the name, we substituted 'm' with a pseudo "micro" (u) symbol. Thus, the name "UISO" was born!

## Design Philosophy

Our primary aim with UISO is to maximize the use of library functions provided by established and respected sources. We take great care to ensure simplicity, and we focus on seamless integration of third-party libraries. Our goal is to build a potent yet easy-to-navigate system that leverages the potential of its underlying technology stack.

## What We Offer

The current version of UISO boasts a host of features, including:

- Capability to read configuration files, connect to a designated WiFi AP, get a timestamp from an internet server, and provide a basic LWM2M service with connection management.
- RTC and sleeptimer.
- SPI driver for Simplelink and SD Card.
- WiFi Service.
- sNTP timestamp synchronization.
- mbedTLS PSK and x509 certificate authentication tested with DTLS.
- Watchdog

## Non-Features

For design, and legal reasons, the following hardware capabilities of the XDK110 will not be supported:

- BLE
- Light Sensor
- Acoustic sensor / Microphone AKU340

## Future Aspirations

We're always striving for improvement, with plans to integrate features such as SipHash, Paho Embedded, and sqlite in future releases. We believe these enhancements will offer a more comprehensive and robust platform for our users.

## License & Disclaimer

The application software of UISO is licensed under the MIT License. However, other software components (either in binary or source form) may have different licenses. Always refer to the corresponding file headers and license files for accurate licensing information.

UISO is not affiliated with LEGIC Identsystems Ltd, the LEGIC XDK Secure Sensor Evaluation Kit, the Rust Foundation or the Rust Project. Furthermore, it's important to note that this codebase should not be used in use cases which have strict safety and/or availability requirements.
