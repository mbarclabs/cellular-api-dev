This repo is a place for working with the new mbed Cellular API on the C030 board.

# Prerequisites

To fetch and build the code in this repository you must first install the [mbed CLI tools](https://github.com/ARMmbed/mbed-cli#installation) and their prerequisites; this should include a compiler (you can use GCC_ARM, ARM or IAR).  You will need to use `mbed config` to tell the mbed CLI tools where that compiler is.

You will also need to obtain a C030 board, at the moment one with a non-NBIoT cellular modem on it.

# How To Use This Code

Clone this repo.

Change directory to this repo and run:

`mbed update`

...to get a version of mbed-os forked by Hasnain Virk that has been forked again to RobMeades Github so that we can play with it.

Change to the `mbed-os` directory and run:

`mbed update cellular_feature_br`

...to switch to use the `cellular_feature_br` branch.

Set the target and the toolchain that you want to use, which should be `UBLOX_C030` and one of `GCC_ARM`, `ARM` or `IAR`.

You can set the target and toolchain for this application once by entering the following two commands (while in the top-level directory of the cloned repo):

`mbed target UBLOX_C030`

`mbed toolchain ARM`

Now you should be able to compile the code with:

`mbed compile`