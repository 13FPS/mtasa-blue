project "discord-rpc"
	targetname "discord-rpc"
	language "C++"
	kind "StaticLib"
	warnings "Off"

	includedirs {
		"discord/include",
		"discord/thirdparty/rapidjson/include"
	}

	defines {
		"DISCORD_DISABLE_IO_THREAD"
	}

	files {
		"premake5.lua",
		"discord/src/discord_rpc.cpp",
		"discord/src/rpc_connection.cpp",
		"discord/src/serialization.cpp",
		"discord/src/connection_win.cpp",
		"discord/src/discord_register_win.cpp",
	}

	if _OPTIONS["with-definitive-adapter"] then
		-- The opt-in x64 Core links the existing RPC implementation; keep
		-- every other unsupported client configuration excluded as before.
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
