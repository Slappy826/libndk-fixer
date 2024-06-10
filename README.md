# libndk_fixer
This fixes the roblox app crashing inside Waydroid when using libndk_translation.


## How to install
- Build yourself or download a precompiled binary (click [here](https://nightly.link/Slappy826/libndk-fixer/workflows/build/master/lib.zip) to download the latest) from ci.
- Ensure you have libndk installed, if not you can install it using [waydroid_script](https://github.com/casualsnek/waydroid_script)
- Edit `/var/lib/waydroid/waydroid_base.prop`
  - Find the line that says `ro.dalvik.vm.native.bridge=libndk_translation.so` and replace the `translation` with `fixer`
- Copy the `libndk_fixer.so` file to this directory `/var/lib/waydroid/overlay/system/lib64/`
- Finished! The app should start normally now.


#### Tested Distributions
- Ubuntu 23.04 and later
- Nobara 39 and later
####
(the above list is not exhaustive and just something me and another person tested with personally)

## Building yourself
- Ensure you have cmake and the [android ndk](https://developer.android.com/ndk/downloads) on your system and the path is stored in the `ANDROID_NDK_HOME` environment variable.
- Clone the repository with `--recursive` to pull the dobby submodule.
- Run build.sh
- The resulting built file `libndk_fixer.so` will be in the build directory.
