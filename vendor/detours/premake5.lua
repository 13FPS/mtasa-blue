project "detours"
    language "C++"
    kind "StaticLib"
	warnings "Off"

    defines {
        "WIN32_LEAN_AND_MEAN",
        -- "_WIN32_WINNT=0x501", Not necessary, predefined in root premake5.lua
    }

    files {
        "4.0.1/src/creatwth.cpp",
        "4.0.1/src/detours.h",
        "4.0.1/src/detours.cpp",
        "4.0.1/src/detver.h",
        "4.0.1/src/image.cpp",
        "4.0.1/src/modules.cpp",
        "4.0.1/src/disolx86.cpp",
        "4.0.1/src/disolx64.cpp",
        "4.0.1/src/disasm.cpp",
    }

    vpaths {
        ["Headers/*"] = "4.0.1/src/**.h",
        ["Sources/*"] = "4.0.1/src/**.cpp",
        ["*"] = "premake5.lua"
    }

    filter "system:not windows"
        flags { "ExcludeFromBuild" }

    -- Only the explicit DE build needs native x64 detours; retain the classic
    -- default exclusions and avoid enabling unrelated architecture/configs.
    if _OPTIONS["with-definitive-adapter"] then
        filter { "architecture:x86_64", "configurations:not Release" }
            flags { "ExcludeFromBuild" }
        filter { "architecture:not x86", "architecture:not x86_64" }
            flags { "ExcludeFromBuild" }
    else
        filter "architecture:not x86"
            flags { "ExcludeFromBuild" }
    end
    filter {}
