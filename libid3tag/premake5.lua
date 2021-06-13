project "libid3tag"
	kind "StaticLib"
	includedirs { ".", "msvc++" }
	files { "*.c", "*.h" }
	defines { "WIN32", "_LIB", "HAVE_CONFIG_H" }
	filter "configurations:Debug"
		defines { "_DEBUG", "DEBUG" }

	filter "configurations:Release"
		defines "NDEBUG"