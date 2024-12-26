# HammerISO
HammerISO is an ISO image that can be used for automated Rowhammer testing of
different systems. In the end of the experiment, a brief overview of the number
of bit flips found by different tools is shown.

In addition to the live system, the ISO image contains a second partition that
is automatically resized to the maximum size at the first boot of the image.
Following boots will not resize the partition. The logs written by the different
tools are compressed and stored in that partition so they can be analyzed
afterwards.

Additionally, the test setup asks the user to upload the results to a server.
It basically takes the compressed results and use `curl` to upload them to the
server currently running at [flippyr.am](https://flippyr.am).

## Building
You can build the current version of the ISO image by running the following
command:
```bash
make
```

Note that, since the ISO building process is based on ArchISO, this requires
ArchLinux in order to build the ISO image.

After building the ISO image is done, it will create a new file `hammeriso.iso`
that can be published and flashed afterwards.

## Flashing
When it is required to flash the image to multiple thumb drives, they can be
connected to the system at the same time. Afterwards, the image can be flashed
to all to them by running the following command:
```bash
sudo sh utilities/flash.sh <device-size> <path/to/ISO/image.iso>
```

Thereby, `<device-size>` specifies the storage size of the thumb drive. Be
aware, this step will flash the image to **ALL CONNECTED USB DEVICE THAT HAVE
THE SPECIFIED SIZE**, so be sure to not have other USB devices with the same
size connected to the system.

The parameter `<path/to/ISO/image.iso>` specifies the path to the ISO image.

For example, the following command can be used to flash to all USB devices with
a size of `57.8G`. **BEWARE: THIS WILL OVERWRITE ALL DEVICES OF THAT SIZE!**
```bash
sudo sh utilities/flash.sh 57.8G hammeriso.iso
```

You can read our privacy statement from here: https://flippyr.am/privacy.html

If you are a developer/researcher and wish to contribute to this project and research, please refer to the [Developer Documentation](./dev-doc.md) for a better understanding of our implementation.