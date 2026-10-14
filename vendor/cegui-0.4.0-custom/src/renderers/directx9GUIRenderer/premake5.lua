project "DirectX9GUIRenderer"
	language "C++"
	kind "StaticLib"
	targetname "DirectX9GUIRenderer"
	warnings "Off"

	defines { "_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING" }

	includedirs {
		"../sdk",
		"../../../include"
	}

	vpaths {
		["Headers/*"] = "**.h",
		["Sources"] = "**.cpp",
		["*"] = "premake5.lua"
	}

	files {
		"premake5.lua",
		"d3d9renderer.cpp",
		"d3d9texture.cpp",
		"../../../include/renderers/d3d9texture.h",
		"../../../include/renderers/d3d9renderer.h"
	}

	if _OPTIONS["with-definitive-adapter"] then
		-- This is the unchanged D3D9 renderer built as a separate x64 artifact;
		-- enabling compilation does not admit it as a DE rendering backend.
		filter { "architecture:x86_64", "configurations:Release" }
			targetdir(buildpath("mta/x64/deps"))
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
