gfx/misc/portal_blueShader
{
	deformVertexes wave 128 sin 0 1 2 1 
	{
		map gfx/misc/electricity.tga
		//blendfunc add
		tcMod rotate 24
		rgbGen entity
	}
	{
		map gfx/misc/electricity.tga
		//blendfunc filter
		blendfunc add
		tcMod rotate 25
		rgbGen entity
	}
	{
		map gfx/misc/electricity_mask.tga
		//blendfunc filter
		blendfunc GL_DST_COLOR GL_SRC_COLOR
	}
}

gfx/misc/portal_redShader
{
	deformVertexes wave 128 sin 0 1 2 1 
	{
		map gfx/misc/electricity_red.tga
		//blendfunc add
		tcMod rotate 24
		rgbGen oneMinusEntity
	}
	{
		map gfx/misc/electricity_red.tga
		//blendfunc filter
		blendfunc add
		tcMod rotate 25
		rgbGen oneMinusEntity
	}
	{
		map gfx/misc/electricity_mask.tga
		//blendfunc filter
		blendfunc GL_DST_COLOR GL_SRC_COLOR
	}
}

