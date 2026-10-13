project "DE Preflight Loader"
	language "C++"
	kind "SharedLib"
	targetname "de_preflight"
	targetsuffix ""
	targetdir(buildpath("mta-launcher/x64/mta"))
	staticruntime "On"
	warnings "Extra"
	fatalwarnings { "All" }
	includedirs { "../../../vendor" }
	files { "Loader.cpp", "Preflight.h", "StrictJson.c", "premake5.lua" }
	links { "json-c", "bcrypt", "shell32" }
	if not _OPTIONS["with-definitive-adapter"] then
		flags { "ExcludeFromBuild" }
	end
	filter { "system:not windows or architecture:not x86_64 or configurations:not Release" }
		flags { "ExcludeFromBuild" }
	filter {}
