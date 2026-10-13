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
	filter { "architecture:x86_64", "configurations:not Release" }
		flags { "ExcludeFromBuild" }
	if not _OPTIONS["with-definitive-adapter"] then
		-- This host exercises the opt-in backend; default solutions should not
		-- implicitly build a backend whose native dependencies are excluded.
		filter "architecture:x86_64"
			flags { "ExcludeFromBuild" }
	end
	filter "system:not windows"
		flags { "ExcludeFromBuild" }
	filter {}

project "MTA Control Synthesis Tests"
	language "C++"
	kind "ConsoleApp"
	targetname "mta_control_synthesis_tests"
	targetdir(buildpath("tests/game_de/%{cfg.architecture}"))
	editandcontinue "Off"
	warnings "Extra"
	flags { "FatalWarnings" }
	includedirs { "../../Shared/sdk" }
	-- Build the same regression suite normally, so sharing Core's input policy
	-- with a new backend does not rely on an external test-only build recipe.
	files { "premake5.lua", "../definitive/MtaControlStateSynthesisTests.cpp" }
	filter "system:not windows"
		flags { "ExcludeFromBuild" }
	filter {}
