project "Game DE"
	language "C++"
	kind "SharedLib"
	targetname "game_de"
	targetdir(buildpath("mta"))
	editandcontinue "Off"
	warnings "Extra"
	flags { "FatalWarnings" }
	defines { "MTA_GAME_DE_EXPORTS" }

	-- This is an MTA backend module. It has no standalone transport, CLEO
	-- lifecycle, classic engine addresses or dependency on game_sa's hooks.
	includedirs { "../../Shared/sdk", "../sdk" }
	files { "premake5.lua", "**.h", "**.cpp" }

	filter "architecture:not x86_64"
		flags { "ExcludeFromBuild" }
	filter "system:not windows"
		flags { "ExcludeFromBuild" }
	filter {}
