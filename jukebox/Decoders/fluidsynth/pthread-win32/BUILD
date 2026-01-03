load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//jukebox:__subpackages__"])

cc_library(
    name = "pthread-win32",
    srcs = ["pthread.c"],
    hdrs = glob(["*.h"]),
    defines = [
        "HAVE_STRUCT_TIMESPEC",
        "PTW32_STATIC_LIB",
        "PTW32_BUILD_INLINED",
    ],
    target_compatible_with = ["@platforms//os:windows"],
    textual_hdrs = glob(["*.c"]),
)
