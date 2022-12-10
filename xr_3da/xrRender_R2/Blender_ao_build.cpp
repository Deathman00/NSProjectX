#include "stdafx.h"
#pragma hdrstop

#include "blender_ao_build.h"
#include "r2_types.h"

CBlender_ao_build::CBlender_ao_build()
{
	description.CLS = 0;
}

CBlender_ao_build::~CBlender_ao_build()
{
}

void CBlender_ao_build::Compile(CBlender_Compile& C)
{
	IBlender::Compile(C);

	switch (C.iElement)
	{
	case 0: //Read secondary RT and blur AO
		C.r_Pass("null", "downsample", FALSE, FALSE, FALSE);
		C.r_Sampler_rtf("s_position", r2_RT_P);
		C.r_End();
		break;
	case 1: // combine
		C.r_Pass("null", "ao_build", FALSE, FALSE, FALSE);
		C.r_Sampler_rtf("s_position", r2_RT_P);
		C.r_Sampler_rtf("s_position_blurred", r2_RT_blurred_position);
		C.r_Sampler_rtf("s_normal", r2_RT_N);
		C.r_Sampler_tex("s_blue_noise", "noise\\blue_noise_texture");
		jitter(C);
		C.r_End();
		break;
	case 2: //Read previous RT and blur AO
		C.r_Pass("null", "ao_filter1", FALSE, FALSE, FALSE);
		C.r_Sampler_rtf("s_ao", r2_RT_ao1);
		C.r_Sampler_rtf("s_position", r2_RT_P);
		C.r_Sampler_rtf("s_position_blurred", r2_RT_blurred_position);
		C.r_End();
		break;
	case 3: //Read secondary RT and blur AO
		C.r_Pass("null", "ao_filter2", FALSE, FALSE, FALSE);
		C.r_Sampler_rtf("s_ao_blurred", r2_RT_ao2);
		C.r_Sampler_rtf("s_position", r2_RT_P);
		C.r_Sampler_rtf("s_position_blurred", r2_RT_blurred_position);
		C.r_End();
		break;
	}
}
