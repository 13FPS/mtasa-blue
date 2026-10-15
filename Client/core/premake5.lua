project "Client Core"
	language "C++"
	kind "SharedLib"
	targetname "core"
	targetdir(buildpath("mta"))
	clangtidy "On"

	-- Disable MSVC incremental linking for Debug.
	filter "configurations:Debug"
		flags { "NoIncrementalLink" }
	filter {}

	filter "system:windows"
		includedirs { "../../vendor/sparsehash/src/windows" }
		buildoptions { "-Zm130" }

	filter {}
		includedirs {
			"../../Shared/sdk",
			".",
			"../sdk",
			"../../vendor/cegui-0.4.0-custom/include",
			"../../vendor/tinygettext",
			"../../vendor/zlib",
			"../../vendor/jpeg-9f",
			"../../vendor/pthreads/include",
			"../../vendor/sparsehash/src/",
			"../../vendor/detours/4.0.1/src",
			"../../vendor/discord-rpc/discord/include",
		}

	pchheader "StdInc.h"
	pchsource "StdInc.cpp"

	-- Keep the adapter consumer independent of classic game/PCH dependencies so
	-- the same MTA-owned lifecycle can be checked by the x64 integration host.
	filter { "files:CGameAdapterModule.cpp" }
		flags { "NoPCH" }
	-- This new translation unit is empty in classic builds. A per-file override
	-- keeps its independent owned-host build free of classic PCH dependencies.
	filter { "files:DefinitiveGuiStartup.cpp" }
		flags { "NoPCH" }
	filter {}

	if _OPTIONS["with-definitive-adapter"] then
		defines { "MTA_WITH_DEFINITIVE_ADAPTER" }
		dependson { "Game DE" }
	end

	filter { "files:FastFailCrashHandler/WerCrashHandler.cpp" }
		flags { "NoPCH" }

	filter {}

	vpaths {
		["Headers/*"] = {"**.h", "**.hpp"},
		["Sources/*"] = "**.cpp",
		["Resources/*"] = {"**.rc", "../launch/resource/mtaicon.ico"},
		["*"] = "premake5.lua"
	}

	links { "detours" }

	files {
		"premake5.lua",
		"../launch/resource/mtaicon.ico",
		"core.rc",
		"**.h",
		"**.hpp",
		"**.cpp"
	}

	links {
		"ws2_32", "d3dx9", "Userenv", "DbgHelp", "xinput", "Imagehlp", "dxguid", "dinput8",
		"strmiids",	"odbc32", "odbccp32", "shlwapi", "winmm", "gdi32", "Imm32", "Psapi", "dwmapi",
		"pthread", "libpng", "jpeg", "zlib", "tinygettext", "discord-rpc", "wintrust", "crypt32",
		"bcrypt",
	}

	defines {
		"INITGUID",
		"PNG_SETJMP_NOT_SUPPORTED",
	}

	prebuildcommands {
		"%[%{!wks.location}/../utils/gen_language_list.exe] %[%{!wks.location}/../Shared/data/MTA San Andreas/MTA/locale] %[languages.generated.h]"
	}

	if _OPTIONS["with-definitive-adapter"] then
		-- Keep the checked x64 Core separate from the classic DLL. Other
		-- architectures/configurations still lack a supported startup path.
		filter { "architecture:x86_64", "configurations:Release" }
			targetdir(buildpath("mta/x64"))
			-- DE startup rejects incomplete game/render backends before any
			-- D3D9 call. Do not require the legacy June 2010 runtime merely to
			-- load Core; retain classic linkage and defer this renderer's import.
			links { "delayimp" }
			-- The separate GUI startup workload creates its own D3D12 objects;
			-- classic builds retain their existing renderer linkage.
			links { "d3d12", "dxgi" }
			linkoptions { "/DELAYLOAD:d3dx9_43.dll" }
			-- The backend loader resolves adjacent modules. Stage real build
			-- outputs together without installing anything into the game.
			postbuildcommands {
				"{COPY} \"%{wks.location}/../Bin/mta/game_de.dll\" \"%{wks.location}/../Bin/mta/x64/\"",
				"{COPY} \"%{wks.location}/../Bin/server/x64/pthread.dll\" \"%{wks.location}/../Bin/mta/x64/\""
			}
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
