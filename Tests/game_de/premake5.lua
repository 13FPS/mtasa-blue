project "Game DE Host Tests"
	language "C++"
	kind "ConsoleApp"
	targetname "game_de_host_tests"
	targetdir(buildpath("tests/game_de"))
	editandcontinue "Off"
	warnings "Extra"
	flags { "FatalWarnings" }
	includedirs { "../../Shared/sdk", "../../Client/sdk", "../../Client/core" }
	files {
		"premake5.lua",
		"../definitive/MtaGameAdapterHostTests.cpp",
		"../../Client/core/CGameAdapterModule.h",
		"../../Client/core/CGameAdapterModule.cpp",
	}
	dependson { "Game DE" }
	filter "architecture:not x86_64"
		flags { "ExcludeFromBuild" }
	filter "system:not windows"
		flags { "ExcludeFromBuild" }
	filter {}
