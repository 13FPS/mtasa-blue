project "Client Launcher"
	language "C++"
	kind "WindowedApp"
	targetname "Multi Theft Auto"
	targetdir(buildpath("."))
	debugdir(buildpath("."))
	clangtidy "On"

	includedirs {
		"../../Shared/sdk",
		"../sdk",
	}

	pchheader "StdInc.h"
	pchsource "StdInc.cpp"

	entrypoint "WinMainCRTStartup"

	vpaths {
		["Headers/*"] = "**.h",
		["Sources/*"] = "**.cpp",
		["Resources/*"] = {"*.rc", "**.ico", "**.xml", "**.manifest"},
		["*"] = "premake5.lua"
	}

	files {
		"premake5.lua",
		"*.h",
		"*.cpp"
	}

	filter "system:windows"
		staticruntime "On"
		files {
			"NEU/gameicon_NEU.ico",
			"NEU/Multi Theft Auto.gdf.xml",
			"launch.rc",
			"Multi Theft Auto.rc",
			"Multi Theft Auto.manifest",
			"resource/mtaicon.ico"
		}

	if _OPTIONS["with-definitive-adapter"] then
		filter { "architecture:x86_64", "configurations:Release" }
			defines { "MTA_DE_LAUNCHER_PREFLIGHT" }
			targetdir(buildpath("mta-launcher/x64"))
			debugdir(buildpath("mta-launcher/x64"))
			-- The engineering CLI has no classic GUI resources or MFC dependency.
			removefiles { "launch.rc", "Multi Theft Auto.rc" }
			dependson { "DE Preflight Loader" }
		filter { "architecture:not x86", "architecture:not x86_64" }
			flags { "ExcludeFromBuild" }
		filter { "architecture:x86_64", "configurations:not Release" }
			flags { "ExcludeFromBuild" }
	else
		filter "architecture:not x86"
			flags { "ExcludeFromBuild" }
	end

	filter "system:not windows"
		flags { "ExcludeFromBuild" }
	filter {}
	include "DefinitivePreflight"
