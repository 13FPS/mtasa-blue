project "Game DE"
	language "C++"
	kind "SharedLib"
	targetname "game_de"
	targetdir(buildpath("mta"))
	editandcontinue "Off"
	warnings "Extra"
	flags { "FatalWarnings" }
	defines { "MTA_GAME_DE_EXPORTS" }
	links { "bcrypt", "detours", "d3d12", "dxgi" }

	-- This is an MTA backend module. It has no standalone transport, CLEO
	-- lifecycle, classic engine addresses or dependency on game_sa's hooks.
	includedirs { "../../Shared/sdk", "../sdk", "../../vendor/detours/4.0.1/src" }
	files { "premake5.lua", "**.h", "**.cpp" }
	if _OPTIONS["with-definitive-adapter"] then
		filter { "architecture:x86_64", "configurations:Release" }
			defines { "DETOURS_MTA_VM_TRANSACTION_NODES" }
	end

	filter "architecture:not x86_64"
		flags { "ExcludeFromBuild" }
	-- The native movement candidate shares Core's explicit Release/x64
	-- experiment and its Detours dependency, rather than enabling new configs.
	filter { "architecture:x86_64", "configurations:not Release" }
		flags { "ExcludeFromBuild" }
	if not _OPTIONS["with-definitive-adapter"] then
		-- Register the backend normally, but do not add its experimental native
		-- hook dependency to default builds that exclude x64 Detours and Core.
		filter "architecture:x86_64"
			flags { "ExcludeFromBuild" }
	end
	filter "system:not windows"
		flags { "ExcludeFromBuild" }
	filter {}

	filter {}
	-- The complete compatible-file service is MTA code shared by the backends;
	-- compiling it does not publish a native/DE audio source or service factory.
	files { "../sdk/game/CAudioContainerFiles.h", "../sdk/game/CAudioContainerFiles.cpp" }
