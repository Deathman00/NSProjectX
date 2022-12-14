#pragma once

#ifdef XRCORE_EXPORTS
#define XRCDB_API XRCORE_API
#else
#define XRCDB_API XRCORE_API
#endif


namespace Opcode 
{
	class OPCODE_Model;
	class AABBNoLeafNode;
};

#pragma pack(push,8)

namespace CDB
{
	// Triangle
	class XRCDB_API TRI						//*** 16 bytes total (was 32 :)
	{
	public:
		u32				verts[3];		// 3*4 = 12b

#ifdef _WIN64

		union 
		{
			u64	dummy;				// 8b

			struct 
			{
				u64		material : 14;		// 
				u64		suppress_shadows : 1;	// 
				u64		suppress_wm : 1;		// 
				u64		sector : 16;			// 
				u64		dumb : 32;
			};

			struct 
			{
				u32 dummy_low;
				u32 dummy_high;
			};
		};
#else

		union 
		{
			u32	dummy;				// 4b

			struct 
			{
				u32		material : 14;		// 
				u32		suppress_shadows : 1;	// 
				u32		suppress_wm : 1;		// 
				u32		sector : 16;			// 
			};
		};
#endif

	public:
		IC u32			IDvert(u32 ID) { return verts[ID]; }
	};


	// Model definition
	class		XRCDB_API		MODEL
	{
		friend class COLLIDER;
		enum
		{
			S_READY = 0,
			S_INIT = 1,
			S_BUILD = 2,
			S_forcedword = u32(-1)
		};
	private:
		xrCriticalSection		cs;
		Opcode::OPCODE_Model*	tree;
		u32						status;		// 0=ready, 1=init, 2=building

											// tris
		TRI*					tris;
		int						tris_count;
		Fvector*				verts;
		int						verts_count;
	public:
		MODEL();
		~MODEL();

		IC Fvector*				get_verts() { return verts; }
		IC int					get_verts_count()	const { return verts_count; }
		IC TRI*					get_tris() { return tris; }
		IC int					get_tris_count()	const { return tris_count; }
		IC void					syncronize()	const
		{
			if (S_READY != status)
			{
				Log("! WARNING: syncronized CDB::query");
				xrCriticalSection*	C = (xrCriticalSection*)&cs;
				C->Enter();
				C->Leave();
			}
		}

		void					build_internal(Fvector* V, int Vcnt, TRI* T, int Tcnt, void* bcp = NULL);
		void					build(Fvector* V, int Vcnt, TRI* T, int Tcnt, void* bcp = NULL);
		u32						memory();
	};

	// Collider result
	struct XRCDB_API RESULT
	{
		Fvector			verts[3];

#ifdef _WIN64

		union 
		{
			u64			dummy;				// 8b

			struct 
			{
				u64		material : 14;		// 
				u64		suppress_shadows : 1;	// 
				u64		suppress_wm : 1;		// 
				u64		sector : 16;			// 
				u64		dumb : 32;
			};

			struct 
			{
				u32 dummy_low;
				u32 dummy_high;
			};
		};
#else

		union 
		{
			u32			dummy;				// 4b
			struct 
			{
				u32		material : 14;		// 
				u32		suppress_shadows : 1;	// 
				u32		suppress_wm : 1;		// 
				u32		sector : 16;			// 
			};
		};
#endif

		int	id;
		float range;
		float u, v;
	};

	// Collider Options
	enum 
	{
		OPT_CULL = (1 << 0),
		OPT_ONLYFIRST = (1 << 1),
		OPT_ONLYNEAREST = (1 << 2),
		OPT_FULL_TEST = (1 << 3)		// for box & frustum queries - enable class III test(s)
	};

	// Collider itself
	class XRCDB_API COLLIDER
	{
		// Ray data and methods
		u32	ray_mode;
		u32	box_mode;
		u32	frustum_mode;

		// Result management
		xr_vector<RESULT>	rd;
	public:
		COLLIDER();
		~COLLIDER();

		ICF void		ray_options(u32 f) { ray_mode = f; }
		void			ray_query(const MODEL *m_def, const Fvector& r_start, const Fvector& r_dir, float r_range = 10000.f);

		ICF void		box_options(u32 f) { box_mode = f; }
		void			box_query(const MODEL *m_def, const Fvector& b_center, const Fvector& b_dim);
		
		ICF RESULT*		r_begin() { return &*rd.begin(); };
		ICF RESULT*		r_end() { return &*rd.end(); };
		RESULT&			r_add();
		void			r_free();
		ICF int			r_count() { return rd.size(); };
		ICF void		r_clear() { rd.clear_not_free(); };
		ICF void		r_clear_compact() { rd.clear_and_free(); };
	};

	struct non_copyable 
	{
		non_copyable() {}
	private:
		non_copyable(const non_copyable &) {}
		non_copyable	&operator=		(const non_copyable &) {}
	};

#pragma warning(push)
#pragma warning(disable:4275)

	const u32 clpMX = 24, clpMY = 16, clpMZ = 24;

	class XRCDB_API CollectorPacked : public non_copyable 
	{
		typedef xr_vector<u32>		DWORDList;
		typedef DWORDList::iterator	DWORDIt;

		xr_vector<Fvector>	verts;
		xr_vector<TRI>		faces;

		Fvector				VMmin, VMscale;
		DWORDList			VM[clpMX + 1][clpMY + 1][clpMZ + 1];
		Fvector				VMeps;

		u32					VPack(const Fvector& V);
	public:
		CollectorPacked(const Fbox &bb, int apx_vertices = 5000, int apx_faces = 5000);


		void				add_face(const Fvector& v0, const Fvector& v1, const Fvector& v2, u16 material, u16 sector);
#ifdef _WIN64
		void				add_face_D(const Fvector& v0, const Fvector& v1, const Fvector& v2, u64 dummy);
#else
		void				add_face_D(const Fvector& v0, const Fvector& v1, const Fvector& v2, u32 dummy);
#endif
		xr_vector<Fvector>& getV_Vec() { return verts; }
		Fvector*			getV() { return &*verts.begin(); }
		size_t				getVS() { return verts.size(); }
		TRI*				getT() { return &*faces.begin(); }
		size_t				getTS() { return faces.size(); }
		void				clear();
	};
#pragma warning(pop)
};

#pragma pack(pop)
