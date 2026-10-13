project "libpng"
    kind "StaticLib"
    language "C"
    architecture "x86"
    systemversion "latest"
	warnings "Off"

    targetdir "bin"
    objdir "obj"

    if _OPTIONS["with-definitive-adapter"] then
        -- The opt-in x64 Core needs a real x64 codec, while separate outputs
        -- prevent either architecture/configuration from replacing another.
        targetdir "bin/%{cfg.architecture}/%{cfg.buildcfg}"
        objdir "obj/%{cfg.architecture}/%{cfg.buildcfg}"
        filter "platforms:x64"
            architecture "x86_64"
        filter {}
    end

    files {
        "*.c",
        "*.h"
    }

    includedirs {
        "../zlib"
    }

    defines {
        "_CRT_SECURE_NO_WARNINGS",
        "WIN32_LEAN_AND_MEAN",
        "PNG_NO_MMX_CODE",
        "PNG_SETJMP_NOT_SUPPORTED"
    }
