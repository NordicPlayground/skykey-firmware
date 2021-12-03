# skykey-firmware

Skykey is a demo project which (aims) to illustrate how the [Zephyr](https://github.com/zephyrproject-rtos/zephyr) and [nRF SDK](https://github.com/nrfconnect/sdk-nrf) codebases can be used to create a physical password safe on the [nRF9160 DK](https://www.nordicsemi.com/Products/Development-hardware/nRF9160-DK/GetStarted). This project features:
* Use of the [Event Manager](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/libraries/others/event_manager.html#event-manager) and the [Common Application Framework (CAF)](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/libraries/caf/caf_overview.html)
* Power management using CAF
* Interfacing with custom peripherals (fingerprint sensor)
* Inter-chip communication on the nRF9160 DK *(still in the works)*
* Simulation of a bluetooth keyboard *(still in the works)*
* UI through a display ([ST7789V](https://www.adafruit.com/product/4313)) powered by [LVGL v7.6.1](https://github.com/lvgl/lvgl/releases/tag/v7.6.1)
* LTE connectivity
* AWS device shadows
* File download from a URL with persistent storage


This project is loosely based off [Asset Tracker v2](https://github.com/nrfconnect/sdk-nrf/tree/38462c95c073106c0c2a70e8f7c6755e3e826094/applications/asset_tracker_v2).
## Initializing the repository

To initialize this repository, call:

``west init -m https://github.com/NordicPlayground/skykey-firmware``

``west update``

This project contains code for both the nRF52840 and the nRF9160 that are on the nRF9160 DK. The code for each of the chips is located in their respective folders in ``./skykey-firmware``

# nRF9160
## Modules
**Modem module:**
Responsible for LTE connectivity.

**Cloud module:** Handles cloud communication (AWS).

**Download module:** Listens to cloud module for a given URL. Downloads a file from the given URL and stores it persistently in the storage flash partition.

**Fingerprint module:** Glue between fingerprint sensor and the rest of the system

**Password module:** Opens password file from storage flash partitions, *(decrypts)* the file and:
* Submits event containing available platforms if it receives `	DISPLAY_EVT_REQUEST_PLATFORMS`
* Submits event containing *(unencrypted???)* password to bluetooth module if it receives `DISPLAY_EVT_PLATFORM_CHOSEN` **(this is a work in progress)**

**Display module:** Handles display-related tasks which allows users to choose a password for the device to input to the PC. The display module itself handles the tracking of state and handling of events, and it is (where possible) kept separate from display-specific code. The code that actually drives the display is found under `src/display`. The library used is [LVGL v7.6.1](https://github.com/lvgl/lvgl/releases/tag/v7.6.1). 

## Utils
To ensure that `download_module.c` does not try to write to the stored file whilst `password_module.c` tries to read from it, `file_util.c` manages any operation related to the stored file.
## Issuing a certificate from AWS IoT
Follow the steps in [creating a thing in AWS IoT](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/samples/nrf9160/aws_fota/README.html#creating-a-thing-in-aws-iot) with `your_client_id` being set to the device's IMEI number. A policy is not necessary. Then follow the steps in [programming the certificates to the on-board modem of the nRF9160-based kit](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/libraries/networking/aws_iot.html#programming-the-certificates-to-the-on-board-modem-of-the-nrf9160-based-kit).


# nRF52840
*Todo: More documentation here.*
## Bluetooth keyboard simulation
The nRF52840 simulates a bluetooth keyboard. It generates HID reports corresponding to button presses, and is even capable of typing multi-keystroke characters, such as the character `^` on a Norwegian keyboard layout. 
# Known issues
**Power management and the ST7789V driver:** 
You might get some compilation errors when trying to build with power management activated. This is caused by a bug in `./zephyr/drivers/display/display_st7789v.c`. To fix this, change line 418 (`case DEVICE_PM_SET_POWER_STATE:`) to `case PM_DEVICE_STATE_SET:`