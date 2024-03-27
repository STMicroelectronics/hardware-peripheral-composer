# hardware-composer #

This module contains the STMicroelectronics android.hardware.graphics.composer3 binary source code.

It is part of the STMicroelectronics delivery for Android.

## Description ##

This module implements android.hardware.graphics.composer3 AIDL version 1.
Please see the Android delivery release notes for more details.

## Documentation ##

* The [release notes][] provide information on the release.
[release notes]: https://wiki.st.com/stm32mpu/wiki/STM32_MPU_OpenSTDroid_release_note_-_v5.1.0

## Dependencies ##

This module can't be used alone. It is part of the STMicroelectronics delivery for Android.

```
PRODUCT_PACKAGES += \
    android.hardware.graphics.composer3-service.stm32mpu
```

## Containing ##

This directory contains the sources and associated Android makefile to generate the composer3 binary.

## License ##

This module is distributed under the Apache License, Version 2.0 found in the [LICENSE](./LICENSE) file.
