package(default_visibility = ["//visibility:public"])

# a convenient target that includes the main classes of the sound library
# you can also use the individual targets in the client side, instead, if you need faster compilation

cc_library(
    name = "jukebox",
    deps = [
		"//jukebox/Mixer:mixer",
		"//jukebox/Sound:sound",
    ],
)
