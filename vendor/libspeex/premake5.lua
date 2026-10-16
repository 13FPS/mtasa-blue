project "libspeex"
	language "C"
	kind "StaticLib"
	targetname "libspeex"
	warnings "Off"

	defines {
		"HAVE_CONFIG_H"
	}

	includedirs {
		".",
		"libspeex"
	}

	vpaths {
		["Headers/*"] = "**.h",
		["Sources/*"] = "**.c",
		["*"] = "premake5.lua"
	}

	files {
		"premake5.lua",
		"speex/*.h",
		"libspeex/*.h",
		"libspeex/*.c",
		"libspeexdsp/*.h",
		"libspeexdsp/*.c"
	}

	excludes {
		-- These files are in libspeex and libspeexdsp
		"libspeexdsp/kiss_fft.c",
		"libspeexdsp/kiss_fftr.c",
		"libspeexdsp/smallft.c",
		"libspeexdsp/fftwrap.c",
	}

	filter "system:windows"
		disablewarnings {
			"4244", -- warning C4244: '=': conversion from '?' to '?', possible loss of data
			"4305", -- warning C4305: 'initializing': truncation from '?' to '?'
			"4018", -- warning C4018: '<': signed/unsigned mismatch
		}

	if _OPTIONS["with-definitive-adapter"] then
		-- Build the existing voice codec independently of the client/game port.
		-- Its x64 library must not replace the classic output or admit the still
		-- unsupported deathmatch, audio-device and network modules.
		filter { "platforms:not x86", "platforms:not x64" }
			flags { "ExcludeFromBuild" }
		filter { "platforms:x64", "configurations:not Release" }
			flags { "ExcludeFromBuild" }
		filter { "platforms:x64", "configurations:Release" }
			targetdir(buildpath("mods/deathmatch/x64"))
	else
		filter "architecture:not x86"
			flags { "ExcludeFromBuild" }
	end

	filter "system:not windows"
		flags { "ExcludeFromBuild" }

	filter {}
