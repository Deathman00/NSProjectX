#include "stdafx.h"
#include "build.h"
#include "common_compilers\xrThread.h"

xrCriticalSection	hemi_CS;
xr_vector<int>		hemi_task_pool;
u32					hemi_count;

class CPrecalcHemisphereThr : public CThread
{
	CDB::COLLIDER DB;
public:
	CPrecalcHemisphereThr(u32 ID) : CThread(ID) { thMessages = TRUE; }

	virtual void	Execute()
	{
		// Priority
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		if (b_highest_priority) SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
		Sleep(0);

		DB.ray_options(0);
		thProgress = 0;

		for (;;)
		{
			u32 it;
			// Get task
			hemi_CS.Enter();
			thProgress = 1.f - float(hemi_task_pool.size()) / float(hemi_count);

			if (hemi_task_pool.empty())
			{
				hemi_CS.Leave();
				return;
			}

			it = hemi_task_pool.back();
			hemi_task_pool.pop_back();
			hemi_CS.Leave();

			base_color_c		vC;
			Vertex*		V = g_vertices[it];

			V->normalFromAdj();
			LightPoint(&DB, RCAST_Model, vC, V->P, V->N, pBuild->L_static, LP_dont_rgb + LP_dont_sun, 0);

			vC.mul(0.5f);
			V->C._set(vC);
		}
	}
};

const	float	aht_max_edge = c_SS_maxsize / 2.5f;	// 2.0f;			// 2 m

bool	is_CCW(int _1, int _2)
{
	if (0 == _1 && 1 == _2)	return true;
	if (1 == _1 && 2 == _2) return true;
	if (2 == _1 && 0 == _2)	return true;
	return	false;
}

// Iterate on edges - select longest
int		callback_edge_longest(Face* F)
{
	float	max_err = -1;
	int		max_id = -1;
	for (u32 e = 0; e<3; e++)
	{
		Vertex					*V1, *V2;
		F->EdgeVerts(e, &V1, &V2);
		float len = V1->P.distance_to(V2->P);	// len
		if (len<aht_max_edge)	continue;
		if (len>max_err)
		{
			max_err = len;
			max_id = e;
		}
	}
	return	max_id;
}

void CBuild::xrPhase_AdaptiveHT()
{
	CDB::COLLIDER	DB;
	DB.ray_options(0);

	Status("Tesselating...");
	
	if (1)
	{
		for (u32 fit = 0; fit<g_faces.size(); fit++) 
		{		// clear split flag from all faces + calculate normals
			g_faces[fit]->flags.bSplitted = false;
			g_faces[fit]->flags.bLocked = true;
			g_faces[fit]->CalcNormal();
		}
		u_Tesselate(callback_edge_longest, 0, 0);		// tesselate
	}

	// Tesselate + calculate
	Status("Precalculating...");

	mem_Compact();

	// Build model
	FPU::m64r();
	BuildRapid(FALSE);

	// Prepare
	FPU::m64r();
	Status("Precalculating : base hemisphere ...");
	mem_Compact();
	Light_prepare();

	// calc approximate normals for vertices + base lighting
	CThreadManager TMPrecalcilatingBaseHemisphere;

	u32 count = g_vertices.size();
	//u32 threads_count = 16;
	extern int			i_thread_count;
	u32 threads_count = i_thread_count;

	if (count)
	{
		CTimer	start_time;
		start_time.Start();
		Progress(0.f);

		hemi_count = count;

		for (u32 it = 0; it<count; it++)	hemi_task_pool.push_back(it);

		for (u32 thID = 0; thID < threads_count; thID++)
			TMPrecalcilatingBaseHemisphere.start(xr_new<CPrecalcHemisphereThr>(thID));

		TMPrecalcilatingBaseHemisphere.wait(500);
		clMsg("%f seconds", start_time.GetElapsed_sec());
	}
}

void CBuild::u_Tesselate(tesscb_estimator* cb_E, tesscb_face* cb_F, tesscb_vertex* cb_V)
{
	// main process
	FPU::m64r();
	Status("Tesselating...");
	g_bUnregister = FALSE;
	xr_vector<Face*> adjacent;	adjacent.reserve(6 * 2 * 3);
	u32		counter_create = 0;
	u32		cnt_verts = g_vertices.size();
	u32		cnt_faces = g_faces.size();
	for (u32 I = 0; I<g_faces.size(); I++)
	{
		Face* F = g_faces[I];
		if (0 == F)				continue;
		if (F->flags.bSplitted) {
			if (!F->flags.bLocked)	FacePool.destroy(g_faces[I]);
			continue;
		}
		if (F->CalcArea()<EPS_L)	continue;

		Progress(float(I) / float(g_faces.size()));
		int		max_id = cb_E(F);
		if (max_id<0)		continue;	// nothing selected

										// now, we need to tesselate all faces which shares this 'problematic' edge
										// collect all this faces
		Vertex			*V1, *V2;
		F->EdgeVerts(max_id, &V1, &V2);
		adjacent.clear();
		for (u32 adj = 0; adj<V1->adjacent.size(); adj++)
		{
			Face* A = V1->adjacent[adj];
			if (A->flags.bSplitted)	continue;
			if (A->VContains(V2))	adjacent.push_back(A);
		}
		std::sort(adjacent.begin(), adjacent.end());
		adjacent.erase(std::unique(adjacent.begin(), adjacent.end()), adjacent.end());

		// create new vertex (lerp)
		counter_create++;
		if (0 == (counter_create % 10000)) {
			for (u32 I = 0; I<g_vertices.size(); I++)	if (0 == g_vertices[I]->adjacent.size())	VertexPool.destroy(g_vertices[I]);
			Status("Working: %d verts created, %d(now) / %d(was) ...", counter_create, g_vertices.size(), cnt_verts);
			FlushLog();
		}

		Vertex*		V = VertexPool.create();
		V->P.lerp(V1->P, V2->P, .5f);

		// iterate on faces which share this 'problematic' edge
		for (u32 af_it = 0; af_it<adjacent.size(); af_it++)
		{
			Face*	AF = adjacent[af_it];
			VERIFY(false == AF->flags.bSplitted);
			AF->flags.bSplitted = true;
			_TCF&	atc = AF->tc.front();

			// indices & tc
			int id1 = AF->VIndex(V1);
			int id2 = AF->VIndex(V2);
			int idB = 3 - (id1 + id2);
			Fvector2			UV;
			UV.averageA(atc.uv[id1], atc.uv[id2]);

			// Create F1 & F2
			Face* F1 = FacePool.create();
			F1->flags.bSplitted = false;
			F1->flags.bLocked = false;
			F1->dwMaterial = AF->dwMaterial;
			F1->dwMaterialGame = AF->dwMaterialGame;
			Face* F2 = FacePool.create();
			F2->flags.bSplitted = false;
			F2->flags.bLocked = false;
			F2->dwMaterial = AF->dwMaterial;
			F2->dwMaterialGame = AF->dwMaterialGame;

			if (is_CCW(id1, id2))
			{
				// F1
				F1->SetVertices(AF->v[idB], AF->v[id1], V);
				F1->AddChannel(atc.uv[idB], atc.uv[id1], UV);
				// F2
				F2->SetVertices(AF->v[idB], V, AF->v[id2]);
				F2->AddChannel(atc.uv[idB], UV, atc.uv[id2]);
			}
			else {
				// F1
				F1->SetVertices(AF->v[idB], V, AF->v[id1]);
				F1->AddChannel(atc.uv[idB], UV, atc.uv[id1]);
				// F2
				F2->SetVertices(AF->v[idB], AF->v[id2], V);
				F2->AddChannel(atc.uv[idB], atc.uv[id2], UV);
			}

			// Normals and checkpoint
			F1->N = AF->N;		if (cb_F)	cb_F(F1);
			F2->N = AF->N;		if (cb_F)	cb_F(F2);

			// don't destroy old face	(it can be used as occluder during ray-trace)
			// if (AF->bLocked)	continue;
			// FacePool.destroy	(g_faces[I]);
		}

		// calc vertex attributes
		{
			V->normalFromAdj();
			if (cb_V)				cb_V(V);
		}
	}

	// Cleanup
	for (u32 I = 0; I<g_faces.size(); I++)	if (0 != g_faces[I] && g_faces[I]->flags.bSplitted)	FacePool.destroy(g_faces[I]);
	for (u32 I = 0; I<g_vertices.size(); I++)	if (0 == g_vertices[I]->adjacent.size())				VertexPool.destroy(g_vertices[I]);
	g_faces.erase(std::remove(g_faces.begin(), g_faces.end(), (Face*)0), g_faces.end());
	g_vertices.erase(std::remove(g_vertices.begin(), g_vertices.end(), (Vertex*)0), g_vertices.end());
	g_bUnregister = true;
}

void CBuild::u_SmoothVertColors(int count)
{
	for (int iteration = 0; iteration<count; iteration++)
	{
		// Gather
		xr_vector<base_color>	colors;
		colors.resize(g_vertices.size());
		for (u32 it = 0; it<g_vertices.size(); it++)
		{
			// Circle
			xr_vector<Vertex*>	circle;
			Vertex*		V = g_vertices[it];
			for (u32 fit = 0; fit<V->adjacent.size(); fit++) {
				Face*	F = V->adjacent[fit];
				circle.push_back(F->v[0]);
				circle.push_back(F->v[1]);
				circle.push_back(F->v[2]);
			}
			std::sort(circle.begin(), circle.end());
			circle.erase(std::unique(circle.begin(), circle.end()), circle.end());

			// Average
			base_color_c		avg, tmp;
			for (u32 cit = 0; cit<circle.size(); cit++)
			{
				circle[cit]->C._get(tmp);
				avg.add(tmp);
			}
			avg.scale(circle.size());
			colors[it]._set(avg);
		}

		// Transfer
		for (u32 it = 0; it<g_vertices.size(); it++)
			g_vertices[it]->C = colors[it];
	}
}
