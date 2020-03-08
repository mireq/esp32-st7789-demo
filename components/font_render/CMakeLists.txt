idf_component_register(
	SRCS
		"freetype2/src/base/ftsystem.c"
		"freetype2/src/base/ftinit.c"
		"freetype2/src/base/ftdebug.c"
		"freetype2/src/base/ftbase.c"
		"freetype2/src/truetype/truetype.c"
		"freetype2/src/sfnt/sfnt.c"
		"freetype2/src/smooth/smooth.c"
		"font_render.c"
	INCLUDE_DIRS
		"include"
		"freetype2/include"
)
target_compile_definitions(${COMPONENT_TARGET} PUBLIC "-DFT2_BUILD_LIBRARY -Wno-unused-function")
