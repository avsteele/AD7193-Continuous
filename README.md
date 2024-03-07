# AD7193 Continuous read example

Simple one-file arduino `.ino` showing how to set up a continuous read
to the AD7193. Code includes reading/writing to a few registers.

## Notes on Usage

Keep in mind: continuous read mode has the device signal on the DOUT/MISO line when a new sample
is ready. This means CS must be kept low once continuous mode is set up. If CS is
high then the device will not pull MISO low. COntinuous mode is required to read channel data faster than ~ 30 Hz.

THe way it works:

1) device pulls MISO low when new data ready
2) user issues SCK pulses to read data from MISO (the easiest way to do this is just write 0s to MOSI)

## Misc

The code was tested with a Digilent Pmod AD5 and a arduino Uno.

With this setup, data output rates match teh datasheet (table 7)

Links:

* <https://www.analog.com/media/en/technical-documentation/data-sheets/AD7193.pdf>
* <https://digilent.com/shop/pmod-ad5-4-channel-4-8-khz-24-bit-a-d-converter/>
* <https://ez.analog.com/data_converters/precision_adcs/f/q-a/24479/ad7193-continuous-read>
