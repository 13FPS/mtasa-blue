project "portaudio"
	language "C++"
	kind "StaticLib"
	targetname "portaudio"
	warnings "Off"

	vpaths {
		["Headers/*"] = "**.h",
		["Sources/*"] = "**.c",
		["*"] = "premake5.lua"
	}

	files {
		"premake5.lua",
		"*.h",
		"*.c"
	}

	filter "system:windows"
		disablewarnings { "4996" }

	if _OPTIONS["with-definitive-client"] then
		-- Build the real microphone dependency from its existing sources only
		-- for the opted-in client port. Its _WIN64 path already omits the x86
		-- inline converters; retain classic outputs for all default builds.
		filter { "architecture:x86_64", "configurations:Release" }
			targetdir(buildpath("mods/deathmatch/x64"))
		filter { "architecture:not x86", "configurations:not Release" }
			flags { "ExcludeFromBuild" }
		filter { "architecture:not x86", "architecture:not x86_64" }
			flags { "ExcludeFromBuild" }
	else
		filter "architecture:not x86"
			flags { "ExcludeFromBuild" }
	end

	filter "system:not windows"
		flags { "ExcludeFromBuild" }
	filter {}
