# skykey-firmware

## Initializing the repository

To initialize this repository, call:

``west init -m https://github.com/NordicPlayground/skykey-firmware``

``west update``

The code itself is then located in ``./skykey-firmware``

## Module description
**Password module:** Opens password file from storage flash partitions, *(decrypts)* the file and:
* Submits event containing *(unencrypted???)* password to bluetooth module if it receives `DISPLAY_EVT_PLATFORM_CHOSEN` **(this is a work in progress)**
* Submits event containing available platforms if it receives `	DISPLAY_EVT_REQUEST_PLATFORMS`


**Download module:** Listens to cloud module for a given URL. Downloads a file from the given URL and stores it persistently in the storage flash partition.

**Cloud module:** Handles modem connection and cloud communication

**Fingerprint module:** Glue between fingerprint sensor and the rest of the system

**Display module:** Handles display-related tasks which allows users to choose a password for the device to input to the PC.

## Issuing a certificate from AWS IoT
Follow the steps in [creating a thing in AWS IoT](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/samples/nrf9160/aws_fota/README.html#creating-a-thing-in-aws-iot) with `your_client_id` being set to the device's IMEI number. A policy is not necessary. Then follow the steps in [programming the certificates to the on-board modem of the nRF9160-based kit](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/libraries/networking/aws_iot.html#programming-the-certificates-to-the-on-board-modem-of-the-nrf9160-based-kit).