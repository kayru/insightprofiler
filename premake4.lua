solution "InsightProfiler"
	language "C++"
	configurations { "Release", "Debug" }
	platforms { "x32" }
	location "build"
	flags {"NoPCH", "NoRTTI", "NoManifest", "ExtraWarnings", "StaticRuntime", "NoExceptions" }
	optimization_flags = { "OptimizeSpeed", "FloatFast", "NoPCH", "ExtraWarnings" }
	includedirs { "." }
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
		targetdir ( "bin/" .. name .."_" ..plat )
	end
end
		
-- SUBPROJECTS

project "insight"	
	kind 			"SharedLib"
	defines			{ "INSIGHT_DLL" }
	links			{ "d3d9", "d3dx9", "dxerr" }
	files 			{ "insight/redist/*.h", "insight/source/*.cpp", "insight/source/*.h", "insight/source/*.inl" }

