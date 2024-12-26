# FlippyRAM
We developed a new framework to check if your system is vulnerable to Rowhammer, incorporating the state-of-the-art Rowhammer techniques and tools. Thus, we invite everyone to participate in this unique opportunity to join forces and close this research gap together. 
We address the following questions: 
- What is the real-world prevalence of the Rowhammer effect? 
- How many systems, in their current configurations, are vulnerable to Rowhammer? 
As large-scale studies with hundreds to thousands of systems are not easy to perform, such a study has not yet been performed.

# Steps to Use FlippyRAM
## 1. Download the ISO Image
- The FlippyRAM project provides an ISO image, HammerISO, for automated Rowhammer testing.
- Download the latest version of the ISO from flippyr.am.

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

## 2. Build the ISO Image (Optional)
If you'd like to build the ISO image yourself:
- Ensure you are running **ArchLinux**.
- Clone the repository:
```
git clone https://github.com/iisys-sns/FlippyRAM.git
cd FlippyRAM
```
- Build the ISO image using:
```bash
make
```

The resulting file, `hammeriso.iso`, will be available in the project directory.

Note: Since the ISO building process is based on **ArchISO**, this requires
**ArchLinux** in order to build the ISO image.

## 3. Flash the ISO Image
- Connect the USB drive(s) to your system.
- Flash the ISO image to all drives of a specific size using the command:
```bash
sudo sh utilities/flash.sh <device-size> <path/to/ISO/image.iso>
```
- Example:
To flash all USB devices of size 57.8G, run:
```bash
sudo sh utilities/flash.sh 57.8G hammeriso.iso
```
**BEWARE: THIS WILL OVERWRITE ALL DEVICES OF THAT SIZE!**

When it is required to flash the image to multiple thumb drives, they can be
connected to the system at the same time. Afterwards, the image will be flashed
to all to them.

The parameter `<device-size>` specifies the storage size of the thumb drive. Be
aware, this step will flash the image to **ALL CONNECTED USB DEVICE THAT HAVE
THE SPECIFIED SIZE**, so be sure to not have other USB devices with the same
size connected to the system.

The parameter `<path/to/ISO/image.iso>` specifies the path to the ISO image.

## 4. Run the Tests
- Boot your target system using the USB drive with the flashed HammerISO.
- The ISO will automatically perform Rowhammer tests and display an overview of the results, including the number of bit flips detected.

## 5. The Results
- Logs/Results are saved on a separate partition of the USB drive where you can easily access anytime.

## 6. Upload Results (Optional)
- After completing the tests, the setup will prompt you to upload the results to our server (Optional).
- Uploading helps contribute to the large-scale study at [flippyr.am](https://flippyr.am).

# Additional Information
## Privacy
Review our privacy policy here: [Privacy Statement](https://flippyr.am/privacy.html)

## Developer Documentation
If you’re a developer or researcher interested in contributing to FlippyRAM, refer to the [Developer Documentation](./dev-doc.md) for implementation details.