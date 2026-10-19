project "Client Deathmatch"
	language "C++"
	kind "SharedLib"
	targetname "client"
	targetdir(buildpath("mods/deathmatch"))
	clangtidy "On"

	pchheader "StdInc.h"
	pchsource "StdInc.cpp"

	defines { "LUNASVG_BUILD", "LUA_USE_APICHECK", "SDK_WITH_BCRYPT" }
	links {
		"Lua_Client", "pcre2", "json-c", "ws2_32", "portaudio", "zlib", "cryptopp", "libspeex", "blowfish_bcrypt", "lunasvg",
		"../../../vendor/bass/lib/bass",
		"../../../vendor/bass/lib/bass_fx",
		"../../../vendor/bass/lib/bassmix",
		"../../../vendor/bass/lib/tags"
	}

	vpaths {
		["Headers/*"] = {"**.h", "../../../Shared/mods/deathmatch/**.h", "../../**.h"},
		["Sources/*"] = {"**.cpp", "../../../Shared/mods/deathmatch/**.cpp", "../../../Shared/**.cpp", "../../../vendor/**.cpp"},
		["*"] = "premake5.lua"
	}

	filter "system:windows"
		includedirs { "../../../vendor/sparsehash/src/windows" }
	filter { "system:windows", "architecture:x86" }
		linkoptions { "/SAFESEH:NO" }

	filter {}
		includedirs {
			"../../../Shared/sdk",
			".",
			"./logic",
			"../../sdk/",
			"../../",
			"../../../vendor/pthreads/include",
			"../../../vendor/bochs",
			"../../../vendor/bass",
			"../../../vendor/libspeex",
			"../../../vendor/zlib",
			"../../../vendor/pcre2",
			"../../../vendor/json-c",
			"../../../vendor/lua/src",
			"../../../Shared/mods/deathmatch/logic",
			"../../../Shared/animation",
			"../../../Shared",
			"../../../vendor/sparsehash/src/",
			"../../../vendor/lunasvg/include"
	}

	files {
		"premake5.lua",
		"**.h",
		"**.cpp",
		"../../../Shared/mods/deathmatch/logic/**.cpp",
		"../../../Shared/mods/deathmatch/logic/**.h",
		"../../../Shared/animation/CEasingCurve.cpp",
		"../../../Shared/animation/CPositionRotationAnimation.cpp",
		-- Todo: Replace these two by using the CryptoPP functions instead
		"../../../vendor/bochs/bochs_internal/bochs_crc32.cpp"
	}

	filter "system:windows"
		buildoptions { "-Zm180" }

	if _OPTIONS["with-definitive-client"] then
		-- Missing imports must not prevent compile-only porting work. Report
		-- them during generation, and reject an accidentally staged x86 archive
		-- before the linker could treat it as an AMD64 dependency.
		local bassRoot = path.getabsolute("../../../vendor/bass/lib/x64")
		local missingImports = {}
		for _, name in ipairs({ "bass", "bass_fx", "bassmix", "tags" }) do
			local library = path.join(bassRoot, name .. ".lib")
			local file = io.open(library, "rb")
			if not file then
				table.insert(missingImports, name .. ".lib")
			else
				local archive = file:read("*a")
				file:close()
				if archive:sub(1, 8) ~= "!<arch>\n" then
					error("DE client dependency is not a COFF archive: " .. library)
				end
				local member, foundAmd64 = 9, false
				local function word(offset)
					local low, high = archive:byte(offset, offset + 1)
					return low and high and low + high * 256 or 0
				end
				while member + 59 <= #archive do
					local size = tonumber(archive:sub(member + 48, member + 57))
					if not size or member + 59 + size > #archive then
						error("DE client dependency has a malformed COFF member: " .. library)
					end
					local data = member + 60
					if size >= 8 then
						local machine = word(data)
						if machine == 0 and word(data + 2) == 0xffff then
							machine = word(data + 6)
						end
						if machine == 0x8664 then
							foundAmd64 = true
						elseif machine == 0x14c or machine == 0xaa64 then
							error("DE client dependency contains a non-AMD64 object: " .. library)
						end
					end
					member = data + size + size % 2
				end
				if not foundAmd64 then
					error("DE client dependency has no AMD64 import objects: " .. library)
				end
			end
		end
		if #missingImports > 0 then
			print("DE client: compile-only work is available; final linking needs AMD64 " .. table.concat(missingImports, ", ") .. " in " .. bassRoot)
		end
		-- Compile the actual existing client only when explicitly requested.
		-- Adapter/Core-only builds remain usable while this larger game-facing
		-- target still has pointer-width work and external audio/network needs.
		filter { "system:windows", "architecture:x86_64", "configurations:Release" }
			targetdir(buildpath("mods/deathmatch/x64"))
			defines { "MTA_WITH_DEFINITIVE_ADAPTER" }
			dependson { "Client Core", "Game DE" }
			-- Truncating an entity/interface pointer cannot be accepted as a
			-- successful x64 port. Keep these failures local to the new target.
			fatalwarnings { "4311", "4302" }
			removelinks {
				"../../../vendor/bass/lib/bass", "../../../vendor/bass/lib/bass_fx",
				"../../../vendor/bass/lib/bassmix", "../../../vendor/bass/lib/tags"
			}
			-- The checked-in BASS import libraries are I386. Use an explicit
			-- AMD64 staging directory; a missing real dependency must fail the
			-- link instead of silently selecting the classic import libraries.
			links {
				"../../../vendor/bass/lib/x64/bass", "../../../vendor/bass/lib/x64/bass_fx",
				"../../../vendor/bass/lib/x64/bassmix", "../../../vendor/bass/lib/x64/tags",
				"winmm", "ole32"
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
	filter {}
