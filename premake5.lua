workspace "minidlna"
	location "build"
	objdir "obj"
	startproject "minidlna"
	staticruntime "on"
	configurations { "Debug", "Release" }

	filter "system:windows"
		systemversion "latest"
		defines { "WIN32", "_WIN32", "WIN_PROFILE_SUPPORT" }

	filter "action:vs*"
		vectorextensions "SSE2"
		defines { "_CRT_SECURE_NO_WARNINGS", "_WINSOCK_DEPRECATED_NO_WARNINGS", "_CRT_NONSTDC_NO_WARNINGS" }

	filter "configurations:Debug"
		symbols "On"
		defines "_DEBUG"
		targetdir "bin/debug"
		runtime "Debug"

	filter "configurations:Release"
		optimize "Size"
		targetdir "bin/release"
		
project "minidlna"
	kind "ConsoleApp"
	rtti "Off"
	files { "*.c", "**.h" }
	files { "tagutils/tagutils.c" }
	excludes { "testupnpdescgen.c", "icons.c", "linux/**" }
	includedirs { "libid3tag" }
	links { "libid3tag", "Strmiids.lib", "ws2_32.lib", "Mf.lib", "Mfplat.lib", "Iphlpapi.lib", "Secur32.lib", "Bcrypt.lib", "mfuuid.lib" }
	
	include "libid3tag"





local function vcpkgStaticTriplet(prj)
	premake.w('<VcpkgTriplet Condition="\'$(Platform)\'==\'Win32\'">x86-windows-static</VcpkgTriplet>')
	premake.w('<VcpkgTriplet Condition="\'$(Platform)\'==\'x64\'">x64-windows-static</VcpkgTriplet>')
end

local function vcpkgStaticTriplet202006(prj)
	premake.w('<VcpkgEnabled>true</VcpkgEnabled>')
    premake.w('<VcpkgUseStatic>true</VcpkgUseStatic>')
	premake.w('<VcpkgAutoLink>true</VcpkgAutoLink>')
end

require('vstudio')

premake.override(premake.vstudio.vc2010.elements, "globals", function(base, prj)
	local calls = base(prj)
	table.insertafter(calls, premake.vstudio.vc2010.targetPlatformVersionGlobal, vcpkgStaticTriplet)
	table.insertafter(calls, premake.vstudio.vc2010.targetPlatformVersionGlobal, disableWinXPWarnings)
	table.insertafter(calls, premake.vstudio.vc2010.globals, vcpkgStaticTriplet202006)
	return calls
end)