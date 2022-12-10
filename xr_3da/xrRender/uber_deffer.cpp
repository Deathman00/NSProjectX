#include "stdafx.h"
#include "uber_deffer.h"
void fix_texture_name(LPSTR fn);

void	uber_deffer	(CBlender_Compile& C, bool hq, LPCSTR _vspec, LPCSTR _pspec, BOOL _aref, LPCSTR _detail_replace, bool DO_NOT_FINISH)
{
	// Uber-parse
	string256		fname,fnameA,fnameB;
	strcpy			(fname,*C.L_textures[0]);	//. andy if (strext(fname)) *strext(fname)=0;
	fix_texture_name(fname);
	ref_texture		_t;		_t.create			(fname);
	bool			bump	= _t.bump_exist		();

	// detect lmap
	bool			lmap	= true;
	if	(C.L_textures.size()<3)	lmap = false;
	else {
		pcstr		tex		= C.L_textures[2].c_str();
		if (tex[0]=='l' && tex[1]=='m' && tex[2]=='a' && tex[3]=='p')	lmap = true	;
		else															lmap = false;
	}


	string256		ps,vs,dt,dtA,dtB;
	strconcat		(sizeof(vs),vs,"deffer_", _vspec, lmap?"_lmh":""	);
	strconcat		(sizeof(ps),ps,"deffer_", _pspec, lmap?"_lmh":""	);
	strcpy_s		(dt,sizeof(dt),_detail_replace?_detail_replace:( C.detail_texture?C.detail_texture:"" ) );

	if	(_aref)		{ strcat(ps,"_aref");	}

	if	(!bump)		{
		fnameA[0] = fnameB[0] = 0;
		strcat			(vs,"_flat");
		strcat			(ps,"_flat");
		if (hq && (C.bDetail_Diffuse || C.bDetail_Bump) )	{
			strcat(vs, "_d");
			strcat(ps, "_d");
		}
	} else {
		strcpy			(fnameA,_t.bump_get().c_str());
		strconcat		(sizeof(fnameB),fnameB,fnameA,"#");
		// KD: forming bump name if detail bump needed
		if (C.bDetail_Bump)
		{
			strcpy_s		(dtA,dt);
			strconcat		(sizeof(dtA),dtA,dtA,"_bump");
			strconcat		(sizeof(dtB),dtB,dtA,"#");
		} else {
			dtA[0] = dtB[0] = 0;
		}
		strcat			(vs,"_bump");
		strcat			(ps,"_bump");
		if (hq && (C.bDetail_Diffuse || C.bDetail_Bump) )	{
			strcat		(vs,"_d"	);
			if (C.bDetail_Bump)
				strcat(ps, "_db"	);	//	bump & detail & hq
			else
				strcat(ps,"_d"		);
		}
	}

	// HQ
	if (bump && hq)		{
		strcat			(vs,"-hq");
		strcat			(ps,"-hq");
	}

	// Uber-construct
	C.r_Pass		(vs,ps,	FALSE);
	C.r_Sampler		("s_base",		C.L_textures[0],	false,	D3DTADDRESS_WRAP,	D3DTEXF_ANISOTROPIC,D3DTEXF_LINEAR,	D3DTEXF_ANISOTROPIC);
	C.r_Sampler		("s_bumpX",		fnameB,				false,	D3DTADDRESS_WRAP,	D3DTEXF_ANISOTROPIC,D3DTEXF_LINEAR,	D3DTEXF_ANISOTROPIC);	// should be before base bump
	C.r_Sampler		("s_bump",		fnameA,				false,	D3DTADDRESS_WRAP,	D3DTEXF_ANISOTROPIC,D3DTEXF_LINEAR,	D3DTEXF_ANISOTROPIC);
	C.r_Sampler		("s_bumpD",		dt,					false,	D3DTADDRESS_WRAP,	D3DTEXF_ANISOTROPIC,D3DTEXF_LINEAR,	D3DTEXF_ANISOTROPIC);
	C.r_Sampler		("s_detail",	dt,					false,	D3DTADDRESS_WRAP,	D3DTEXF_ANISOTROPIC,D3DTEXF_LINEAR,	D3DTEXF_ANISOTROPIC);
	// KD: samplers for detail bump registering
	if (bump && hq && C.bDetail_Bump) {
		C.r_Sampler		("s_detailBump",		dtA,		false,	D3DTADDRESS_WRAP,	D3DTEXF_ANISOTROPIC,D3DTEXF_LINEAR,	D3DTEXF_ANISOTROPIC);
		C.r_Sampler		("s_detailBumpX",		dtB,		false,	D3DTADDRESS_WRAP,	D3DTEXF_ANISOTROPIC,D3DTEXF_LINEAR,	D3DTEXF_ANISOTROPIC);
	}
	if (lmap)C.r_Sampler("s_hemi",	C.L_textures[2],	false,	D3DTADDRESS_CLAMP,	D3DTEXF_LINEAR,		D3DTEXF_NONE,	D3DTEXF_LINEAR);

#ifdef ADVANCED_BUILD
	C.r_Sampler("s_spec",		strconcat(sizeof(fname), fname, fname, "_spec"),		false, D3DTADDRESS_WRAP, D3DTEXF_ANISOTROPIC, D3DTEXF_LINEAR, D3DTEXF_ANISOTROPIC); // <- Very big thanks for LVutner (albedo_spec.dds)
	C.r_Sampler("s_speccolor",	strconcat(sizeof(fname), fname, fname, "_speccolor"),	false, D3DTADDRESS_WRAP, D3DTEXF_ANISOTROPIC, D3DTEXF_LINEAR, D3DTEXF_ANISOTROPIC); // <- Very big thanks for LVutner (albedo_speccolor.dds)
	C.r_Sampler("s_nmap",		strconcat(sizeof(fname), fname, fname, "_nmap"),		false, D3DTADDRESS_WRAP, D3DTEXF_ANISOTROPIC, D3DTEXF_LINEAR, D3DTEXF_ANISOTROPIC); // <- Very big thanks for LVutner (albedo_nmap.dds)
	C.r_Sampler("s_detail_nmap",		strconcat(sizeof(dt),	 dt,	dt,		"_nmap"),		false, D3DTADDRESS_WRAP, D3DTEXF_ANISOTROPIC, D3DTEXF_LINEAR, D3DTEXF_ANISOTROPIC);
#endif

	if (!DO_NOT_FINISH)		C.r_End	();
}
