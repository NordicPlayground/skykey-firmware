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