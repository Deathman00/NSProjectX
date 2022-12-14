#include "stdafx.h"
#include "build.h"
#include "common_compilers\communicate.h"

#include <thread>


CDB::MODEL*	RCAST_Model	= 0;

IC bool				FaceEqual(Face& F1, Face& F2)
{
	// Test for 6 variations
	if ((F1.v[0]==F2.v[0]) && (F1.v[1]==F2.v[1]) && (F1.v[2]==F2.v[2])) return true;
	if ((F1.v[0]==F2.v[0]) && (F1.v[2]==F2.v[1]) && (F1.v[1]==F2.v[2])) return true;
	if ((F1.v[2]==F2.v[0]) && (F1.v[0]==F2.v[1]) && (F1.v[1]==F2.v[2])) return true;
	if ((F1.v[2]==F2.v[0]) && (F1.v[1]==F2.v[1]) && (F1.v[0]==F2.v[2])) return true;
	if ((F1.v[1]==F2.v[0]) && (F1.v[0]==F2.v[1]) && (F1.v[2]==F2.v[2])) return true;
	if ((F1.v[1]==F2.v[0]) && (F1.v[2]==F2.v[1]) && (F1.v[0]==F2.v[2])) return true;
	return false;
}

void SaveUVM			(LPCSTR fname, xr_vector<b_rc_face>& vm)
{
	IWriter* W			= FS.w_open(fname);
	string256 tmp;
	// vertices
	for (u32 v_idx=0; v_idx<vm.size(); v_idx++){
		b_rc_face& rcf	= vm[v_idx];
		sprintf			(tmp,"f %d %d [%3.2f,%3.2f]-[%3.2f,%3.2f]-[%3.2f,%3.2f]",rcf.dwMaterial,rcf.dwMaterialGame,
						rcf.t[0].x,rcf.t[0].y, rcf.t[1].x,rcf.t[1].y, rcf.t[2].x,rcf.t[2].y);
		W->w_string		(tmp);
	}
	FS.w_close	(W);
}

void CBuild::BuildRapid		(BOOL bSaveForOtherCompilers)
{
	float	p_total = 0;
	float	p_cost = 1.f / (g_faces.size());

	xr_delete(RCAST_Model);

	Status("Converting faces...");

	for (u32 fit = 0; fit<g_faces.size(); fit++)	g_faces[fit]->flags.bProcessed = false;

	xr_vector<Face*>	adjacent;	adjacent.reserve(6 * 2 * 3);
	CDB::CollectorPacked	CL(scene_bb, g_vertices.size(), g_faces.size());

	for (vecFaceIt it = g_faces.begin(); it != g_faces.end(); ++it)
	{
		Face*	F = (*it);
		Shader_xrLC&	SH = F->Shader();
		
		if (!SH.flags.bLIGHT_CastShadow)					continue;

		Progress(float(it - g_faces.begin()) / float(g_faces.size()));

		// Collect
		adjacent.clear();
		
		for (int vit = 0; vit<3; vit++)
		{
			Vertex* V = F->v[vit];
		
			for (u32 adj = 0; adj<V->adjacent.size(); adj++)
			{
				adjacent.push_back(V->adjacent[adj]);
			}
		}

		std::sort(adjacent.begin(), adjacent.end());
		adjacent.erase(std::unique(adjacent.begin(), adjacent.end()), adjacent.end());

		// Unique
		BOOL			bAlready = FALSE;	

		for (u32 ait = 0; ait<adjacent.size(); ait++)
		{
			Face*	Test = adjacent[ait];
			
			if (Test == F)					continue;
			if (!Test->flags.bProcessed)	continue;
			
			if (FaceEqual(*F, *Test)) 
			{
				bAlready = TRUE;
				break;
			}
		}
		
		if (!bAlready)
		{
			F->flags.bProcessed = true;
#ifdef _WIN64
			CL.add_face_D(F->v[0]->P, F->v[1]->P, F->v[2]->P, *((u64*)&F));
#else
			CL.add_face_D(F->v[0]->P, F->v[1]->P, F->v[2]->P, *((u32*)&F));
#endif
		}
	}

	// Export references
	if (bSaveForOtherCompilers)		Phase	("Building rcast-CFORM-mu model...");
	Status					("Models...");

	u32 MRS = mu_refs.size();
	float fProgr = 0;
	float fStep = 1.f / MRS;
	Progress(fProgr);

		for (int ref = 0; ref < MRS; ref++)
		{
				mu_refs[ref]->export_cform_rcast(CL);
				fProgr += fStep;
				Progress(fProgr);
		}	 

/*	auto LodThreadsManager = [](const CBuild *BLD, CDB::CollectorPacked &CLP, const int from, const int to)
	{
		//CDB::CollectorPacked CLL = CLP;

		for (int ref = from; ref < to; ref++)
		{
			BLD->mu_refs[ref]->export_cform_rcast(CLP);
		}
	};

	std::thread thread_1(LodThreadsManager, *this, CL, 0, MRS/2);
	std::thread thread_2(LodThreadsManager, *this, CL, MRS/2, MRS);
	thread_1.join();
	thread_2.join();
	*/

	// "Building tree..
	Status					("Building search tree...");
	RCAST_Model				= xr_new<CDB::MODEL> ();
	RCAST_Model->build		(CL.getV(),(int)CL.getVS(),CL.getT(),(int)CL.getTS());

	extern void SaveAsSMF			(LPCSTR fname, CDB::CollectorPacked& CL);
	
	// save source SMF
	string_path				fn;

	if (g_params.m_quality!=ebqDraft) 
		SaveAsSMF(strconcat(sizeof(fn),fn,pBuild->path,"build_cform_source.smf"),CL);

	// Saving for AI/DO usage
	if (bSaveForOtherCompilers)
	{
		Status					("Saving...");
		string_path				fn;

		IWriter*		MFS		= FS.w_open	(strconcat(sizeof(fn),fn,pBuild->path,"build.cform"));
		xr_vector<b_rc_face>	rc_faces;
		rc_faces.resize			(CL.getTS());
		// Prepare faces
		for (u32 k=0; k<CL.getTS(); k++)
		{
			CDB::TRI* T			= CL.getT()+k;
			base_Face* F		= (base_Face*)(*((void**)&T->dummy));
			b_rc_face& cf		= rc_faces[k];
			cf.dwMaterial		= F->dwMaterial;
			cf.dwMaterialGame	= F->dwMaterialGame;
			Fvector2*	cuv		= F->getTC0	();
			cf.t[0].set			(cuv[0]);
			cf.t[1].set			(cuv[1]);
			cf.t[2].set			(cuv[2]);
		}

		if (g_params.m_quality!=ebqDraft)
			SaveUVM	(strconcat(sizeof(fn),fn,pBuild->path,"build_cform_source.uvm"),rc_faces);

		MFS->open_chunk			(0);

		// Header
		hdrCFORM hdr;
		hdr.version				= CFORM_CURRENT_VERSION;
		hdr.vertcount			= (u32)CL.getVS();
		hdr.facecount			= (u32)CL.getTS();
		hdr.aabb				= scene_bb;
		MFS->w					(&hdr,sizeof(hdr));

		// Data
		MFS->w					(CL.getV(),(u32)CL.getVS()*sizeof(Fvector));
/*		uncomment this in case of compatibility needs with old xrDO_Light
#ifdef _WIN64
		size_t t_size = CL.getTS();
		for (size_t i = 0; i < t_size; ++i)
		{
			CDB::TRI *tri = &(CL.getT()[i]);
			MFS->w(&(tri->verts[0]), 12);
			MFS->w_u32(tri->dummy_low);
		}
#else*/
		MFS->w			(CL.getT(),(u32)CL.getTS()*sizeof(CDB::TRI));
//#endif
		MFS->close_chunk		();

		MFS->open_chunk			(1);
		MFS->w					(&*rc_faces.begin(),(u32)rc_faces.size()*sizeof(b_rc_face));
		MFS->close_chunk		();
	}
}
