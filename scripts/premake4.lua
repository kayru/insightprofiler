solution "InsightProfiler"
	language "C++"
	configurations { "Release", "Debug" }
	platforms { "x32" }
	location ( "../build/" .. _ACTION )
	flags {"NoPCH", "NoRTTI", "NoManifest", "ExtraWarnings", "StaticRuntime", "NoExceptions" }
	optimization_flags = { "OptimizeSpeed", "FloatFast", "NoPCH", "ExtraWarnings" }
	includedirs { ".." }
	defines { "_HAS_EXCEPTIONS=0", "_STATIC_CPPLIB" }
	
-- CONFIGURATIONS
	
configuration "Debug"	
	defines     "_DEBUG"
	flags       { "Symbols" }
	
configuration "Release"	
	defines     "NDEBUG"
	flags       { optimization_flags }	
	
--  give each configuration/platform a unique output directory (must happen before projects are defined)

for _, name in ipairs(configurations()) do	
	for _, plat in ipairs(platforms()) do	
		configuration { name, plat }
		targetdir ( "../bin/" .. _ACTION .. "_"  .. name  .. "_" .. plat )
		objdir    ( "../bin/" .. _ACTION .. "_"  .. name  .. "_" .. plat .. "/obj" )
	end
end
		
-- SUBPROJECTS

project "insight"	
	kind 			"SharedLib"
	uuid 			"1D251D82-D63A-41a3-BA5B-D49984DF809E"
	defines			{ "INSIGHT_DLL" }
	links			{ "d3d9", "d3dx9", "dxerr" }
	files 			{ "../insight/redist/*.h", "../insight/source/*.cpp", "../insight/source/*.h", "../insight/source/*.inl" }


project "test-single_thread"
	kind 			"ConsoleApp"
	uuid 			"DC27DD14-B07C-4c29-9C04-0863080C2068"
	links			{ "insight" }
	files 			{ "../tests/test-single_thread.cpp" }


project "test-single_thread_async"
	kind 			"ConsoleApp"
	uuid 			"435D37F9-0E5C-45f0-9876-8A51F9FAAE26"
	links			{ "insight" }
	files 			{ "../tests/test-single_thread_async.cpp" }


project "test-multiple_threads"
	kind 			"ConsoleApp"
	uuid 			"703530D4-FD5B-4b1c-9D1A-5E47944DD31E"
	links			{ "insight" }
	files 			{ "../tests/test-multiple_threads.cpp" }
