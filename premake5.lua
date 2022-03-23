workspace "Filesystem Drivers"
    architecture "x64"
    startproject "Test"

    configurations
    {
        "Debug",
        "Release",
        "Distribution"
    }

    flags
	{
		"MultiProcessorCompile"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

group "Drivers"

project "FAT32"
    location "FAT32"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "off"
    
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp"
    }

    defines
	{
		"_CRT_SECURE_NO_WARNINGS"
    }

    includedirs
    {
        "%{prj.location}/src"
    }
    
    filter "system:windows"
        systemversion "latest"
    
    filter "configurations:Debug"
        defines "DEBUG"
        runtime "Debug"
        symbols "on"
    
    filter "configurations:Release"
        defines "RELEASE"
        runtime "Release"
        optimize "on"

project "Ext2"
    location "Ext2"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "off"
    
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp"
    }

    defines
	{
		"_CRT_SECURE_NO_WARNINGS"
    }

    includedirs
    {
        "%{prj.location}/src"
    }
    
    filter "system:windows"
        systemversion "latest"
    
    filter "configurations:Debug"
        defines "DEBUG"
        runtime "Debug"
        symbols "on"
    
    filter "configurations:Release"
        defines "RELEASE"
        runtime "Release"
        optimize "on"

project "exFAT"
    location "exFAT"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "off"
    
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp"
    }

    defines
    {
        "_CRT_SECURE_NO_WARNINGS"
    }

    includedirs
    {
        "%{prj.location}/src"
    }
    
    filter "system:windows"
        systemversion "latest"
    
    filter "configurations:Debug"
        defines "DEBUG"
        runtime "Debug"
        symbols "on"
    
    filter "configurations:Release"
        defines "RELEASE"
        runtime "Release"
        optimize "on"

group ""

project "Test"
    location "Test"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    staticruntime "off"
    
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp"
    }

    includedirs
    {
        "%{prj.location}/src",
        "FAT32/src",
        "Ext2/src",
        "exFAT/src"
    }

    defines
	{
		"_CRT_SECURE_NO_WARNINGS"
	}

    links
    {
        "FAT32",
        "Ext2",
        "exFAT"
    }

    filter "system:windows"
        systemversion "latest"
    
    filter "configurations:Debug"
        defines "DEBUG"
        runtime "Debug"
        symbols "on"
    
    filter "configurations:Release"
        defines "RELEASE"
        runtime "Release"
        optimize "on"