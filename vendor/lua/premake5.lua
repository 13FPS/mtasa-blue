project "Lua_Server"
	language "C++"
	targetname "lua5.1"
	warnings "Off"

	vpaths {
		["Headers"] = "**.h",
		["Sources"] = "**.c",
		["*"] = "premake5.lua"
	}

	files {
		"premake5.lua",
		"src/**.c",
		"src/**.h",
	}

	defines { "LUA_BUILD_AS_DLL" }

	filter "system:windows"
		kind "SharedLib"
		targetdir(buildpath("server/mods/deathmatch"))

	filter "system:not windows"
		kind "StaticLib"

	filter {"system:windows", "platforms:x64"}
		targetdir(buildpath("server/x64"))

	filter {"system:windows", "platforms:arm"}
		targetdir(buildpath("server/arm"))

	filter {"system:windows", "platforms:arm64"}
		targetdir(buildpath("server/arm64"))


if os.target() == "windows" then
	project "Lua_Client"
		language "C++"
		kind "SharedLib"
		targetname "lua5.1c"
		targetdir(buildpath("mods/deathmatch"))

		vpaths {
			["Headers"] = "**.h",
			["Sources"] = "**.c",
			["*"] = "premake5.lua"
		}

		files {
			"premake5.lua",
			"src/**.c",
			"src/**.h",
		}

		defines {
			"LUA_USE_APICHECK",
			"LUA_BUILD_AS_DLL"
		}

        if _OPTIONS["with-definitive-adapter"] then
            -- Retire the real client Lua dependency independently of the game
            -- module. Keep its x64 artifact away from the classic x86 DLL;
            -- availability does not admit deathmatch or imply gameplay support.
            filter { "platforms:not x86", "platforms:not x64" }
                flags { "ExcludeFromBuild" }
            filter { "platforms:x64", "configurations:not Release" }
                flags { "ExcludeFromBuild" }
            filter { "platforms:x64", "configurations:Release" }
                targetdir(buildpath("mods/deathmatch/x64"))
        else
            filter "platforms:not x86"
                flags { "ExcludeFromBuild" }
        end
        filter {}
end
