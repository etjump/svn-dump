// models_weapons2.shader
// generated by ShaderCleaner on Thu Feb  6 12:43:32 2003
// 6 total shaders
// Tesla Shaders 

models/weapons2/tesla/base_tesla4
{
	{
		map textures/effects/envmap_gold.tga
		rgbGen lightingdiffuse
		tcGen environment
	}
	{
		map models/weapons2/tesla/base_tesla4.tga
		blendFunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
		rgbGen lightingdiffuse
	}
}

models/weapons2/tesla/zap_scroll2b
{
        q3map_surfacelight	1000
        surfaceparm	trans
	surfaceparm nomarks
	surfaceparm nolightmap
//	qer_editorimage models/weapons2/tesla/zap_scroll.tga
	cull none
	
	{
		map models/weapons2/tesla/zap_scroll.tga
		blendFunc GL_ONE GL_ONE
                rgbgen wave triangle .8 2 0 7
                tcMod scroll 0 1.2
	}	
        {
		map models/weapons2/tesla/zap_scroll.tga
		blendFunc GL_ONE GL_ONE
                rgbgen wave triangle 1 1.4 0 5
                tcMod scale  -1 1
                tcMod scroll 0 1.2
	}	
        {
		map models/weapons2/tesla/zap_scroll2a.tga
		blendFunc GL_ONE GL_ONE
                rgbgen wave triangle 1 1.4 0 6.3
                tcMod scale  -1 1
                tcMod scroll 2 1.2
	}	
        {
		map models/weapons2/tesla/zap_scroll2a.tga
		blendFunc GL_ONE GL_ONE
                rgbgen wave triangle 1 1.4 0 7.7
                tcMod scroll -1.3 1.2
	}	
}

