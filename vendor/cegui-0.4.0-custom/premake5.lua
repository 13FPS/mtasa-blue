project "CEGUI"
	language "C++"
	kind "StaticLib"
	targetname "CEGUI"
	warnings "Off"

	pchheader "StdInc.h"
	pchsource "src/StdInc.cpp"

	includedirs {
		"include",
		"dependencies/include",
		"../freetype/include"
	}

	links { "freetype" }

	defines {
		"CEGUIBASE_EXPORTS",
		"_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING"
	}

	vpaths {
		["Headers/*"] = "include/**.h",
		["Sources/*"] = "src/**.cpp",
		["Sources/*"] = "src/**.c",
		["*"] = "premake5.lua"
	}

	files {
		"premake5.lua",
		"src/**.cpp",
		"src/**.c",
		"include/**.h",
	}

	excludes {
		"src/renderers/**",
		"src/pcre/ucptypetable.c",
		"src/pcre/ucptable.c",
		"src/pcre/ucp.c"
	}

	filter "files:src/tinyxml/**.cpp"
		flags { "NoPCH" }
	filter "files:src/pcre/**.c"
		flags { "NoPCH" }

	if _OPTIONS["with-definitive-adapter"] then
		-- Keep the opt-in pointer-width build isolated from classic libraries.
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
		linkoptions { "/ignore:4221" }
		disablewarnings { "4221" }
