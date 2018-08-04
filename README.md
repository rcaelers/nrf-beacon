This is a simple beacon that focuses on privacy. Unlike beacons like iBeacon and Eddystone, this beacon does not advertise any data.

The beacon uses a random private resolvable address that changes (by default) every 15 minutes. The Identity Resolving Key (IRK) can be used to verify that a BLE address belongs to the beacon.

Press and hold the button to control the beacon:


| After   | Led will flash  | What                                                         |
| ------- | :-------------: | :-----                                                       |
| 2 s     | 2 x             | Beacon will become connectable. Allows changing of settings. |
| 5 s     | 3 x             | The beacon will reset all its bonds                          |
| 10 s    | 4 x             | The beacon will reset to default configuration               |
| 15 s    | 2 x             | The beacon will restart                                      |
| 20 s    | 1 x             | No action is peformed                                        |
 
# Building

Set environment variables to point to SDK and compilers:
```
export SDK_ROOT=${HOME}/tmp/nRF5_SDK_15.0.0_a53641a/
export GNU_INSTALL_ROOT=${HOME}/opt/gcc-arm-none-eabi-6-2017-q2-update/bin/
```

## Bootloader

```
cd bootloader
nrfutil keys generate private.pem
nrfutil keys display --key pk --format code private.pem --out_file dfu_public_key.c
```

Build and flash bootloader:
```
make -f boards/holyiot/Makefile  flash
```

## Application

Edit settings in ```config.h```. This file contains the default PIN code and IRK.

Build and flash application:
```
cd application

make -f boards/holyiot/Makefile
make -f boards/holyiot/Makefile flash_softdevice
nrfutil settings generate --family NRF52 --application _build/nrf52832_xxaa.hex --application-version 1 --bootloader-version 1 --bl-settings-version 1 settings.hex 
mergehex --merge _build/nrf52832_xxaa.hex settings.hex --output all.hex 
nrfjprog --program all.hex --sectorerase 
nrfjprog -f nrf52 --reset
```

Create OTA firmware update packages:

```
nrfutil pkg generate --application _build/nrf52832_xxaa.hex --application-version 1 --application-version-string "1.0.0" --hw-version 52 --sd-req 0xA8 --key-file ../bootloader/private.pem FW.zip
```

Or combined with softdevice:
```
nrfutil pkg generate --application _build/nrf52832_xxaa.hex --application-version 1 --application-version-string "1.0.0" --hw-version 52 --sd-req 0xA8 --sd-id 0xA8 --softdevice s132_nrf52_6.0.0_softdevice.hex --key-file ../bootloader/private.pem FW.zip
```

Perform DFU (using [nrf-dfu](https://github.com/rcaelers/nrf-dfu))

```
nrf-dfu dfu --address <mac-address> --firmware FW.zip
```
