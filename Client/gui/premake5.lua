project "GUI"
	language "C++"
	kind "SharedLib"
	targetname "cgui"
	targetdir(buildpath("mta"))
	clangtidy "On"

	filter "system:windows"
		includedirs { "../../vendor/sparsehash/src/windows" }

	filter {}
		includedirs {
			"../../Shared/sdk",
			"../sdk",
			"../../vendor/cegui-0.4.0-custom/include",
			"../../vendor/sparsehash/src/"
		}

	pchheader "StdInc.h"
	pchsource "StdInc.cpp"

	defines {
		"_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING"
	}

	links {
		"CEGUI", "DirectX9GUIRenderer", "Falagard",
		"d3dx9.lib",
		"dxerr.lib"
	}

	vpaths {
		["Headers/*"] = "**.h",
		["Sources/*"] = "**.cpp",
		["*"] = "premake5.lua"
	}

	files {
		"premake5.lua",
		"*.h",
		"*.cpp"
	}

	if _OPTIONS["with-definitive-adapter"] then
		-- Build the existing GUI independently before any DE renderer port.
		-- Its D3D9 interface and classic output remain unchanged.
		filter { "architecture:x86_64", "configurations:Release" }
			targetdir(buildpath("mta/x64"))
			-- No GUI source calls DXErr. Its obsolete SDK library is absent
			-- from the verified D3DX package; preserve classic linkage only.
			removelinks { "dxerr.lib" }
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
