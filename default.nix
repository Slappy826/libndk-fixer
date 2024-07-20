{ stdenv
, androidenv
, cmake
, lib
}:

let
  androidComposition = androidenv.composeAndroidPackages {
    includeEmulator = false;
    includeNDK = true;
    includeSources = false;
    useGoogleAPIs = false;
    useGoogleTVAddOns = false;
  };
in
stdenv.mkDerivation {
  pname = "libndk_translation_fixer";
  version = "1.0.0";

  src = ./.;

  nativeBuildInputs = [ cmake ];

  ANDROID_NDK_HOME =
    "${androidComposition.androidsdk}/libexec/android-sdk/ndk-bundle";

  doCheck = false;

  meta = {
    description =
      "libndk_translation replacement to fix Roblox crashing inside Waydroid";
    license = lib.licenses.mit;
    maintainers = with lib.maintainers; [ alexoundos ];
    platforms = [ "x86_64-linux" ];
  };
}

