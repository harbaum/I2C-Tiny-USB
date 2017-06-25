# I2C-Tiny-USB on Digispark

The i2c-tiny-usb firmware has been ported to the
[digispark](http://digistump.com/products/1). The original port is
available [here](https://github.com/nopdotcom/i2c_tiny_usb-on-Little-Wire).

I reverted the changes of the USB ID to make this compatible again
with the original i2c-tiny-usb driver.

![Foto](digispark_rc522.jpg)

## Pin mapping

```SDA``` is mapped to pin P0 of the digispark. ```SCL``` is mapped to P2.

## Flashing the firmware

A compiled binary named ```main.hex``` is available in the repository.

It can be flashed to the digispark board using the ```micronucleus``` tool
from the digispark arduino installation:

```
micronucleus --run --dump-progress --type intel-hex main.hex
```

More details on this can be found [here](https://github.com/nopdotcom/i2c_tiny_usb-on-Little-Wire/wiki/BuildingOnLinux).
