project "lunasvg"
	language "C++"
	kind "StaticLib"
	targetname "lunasvg"
	targetdir(buildpath("mta"))
	floatingpoint "Fast"
	rtti "Off"
	warnings "Off"

	defines {
		"PLUTOVG_BUILD",
		"LUNASVG_BUILD",
		"_CRT_SECURE_NO_WARNINGS"
	}

	vpaths {
		["Headers"] = "source/**.h",
		["Headers/*"] = "include/**.h",
		["Headers/plutovg/*"] = "plutovg/**.h",
		["Sources"] = "source/**.cpp",
        ["Sources/*"] = "source/**.c",
		["Sources/plutovg"] = "plutovg/**.cpp",
		["Sources/plutovg/*"] = "plutovg/**.c",
		["*"] = "premake5.lua"
	}

	files {
		"premake5.lua",
		"**.cpp",
		"**.h",
		"**.c"
	}

	includedirs {
		"plutovg/include",
		"include",
		"source"
	}

	filter "system:windows"
		disablewarnings {
			"4244", -- warning C4244: '=': conversion from '?' to '?', possible loss of data
			"4018", -- warning C4018: '<': signed/unsigned mismatch
		}

	if _OPTIONS["with-definitive-client"] then
		-- Reuse the existing SVG renderer in the real client graph. Keep its
		-- AMD64 archive separate so building the port cannot replace x86 data.
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
