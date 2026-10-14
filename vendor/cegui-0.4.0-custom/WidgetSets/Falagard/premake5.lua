project "Falagard"
	language "C++"
	kind "StaticLib"
	targetname "Falagard"
	warnings "Off"

	pchheader "StdInc.h"
	pchsource "src/StdInc.cpp"

	defines { "FALAGARDBASE_EXPORTS", "_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING" }

	includedirs {
		"include",
		"../../include" -- CEGUI Includes
	}

	vpaths {
		["Headers/*"] = "**.h",
		["Sources"] = "**.cpp",
		["*"] = "premake5.lua"
	}

	files {
		"premake5.lua",
		"src/**.cpp",
		"include/**.h",
	}

	if _OPTIONS["with-definitive-adapter"] then
		-- Reuse the existing widget implementation without replacing its ABI.
		filter { "architecture:x86_64", "configurations:Release" }
			targetdir(buildpath("mta/x64/deps"))
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

	filter {"system:windows"}
		disablewarnings { "4297" }
