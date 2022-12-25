#pragma once

#include "../sk_gpu.h"

#include <math.h>
#include <malloc.h>

static inline float vec3_magnitude_sq(const float *a) { return a[0] * a[0] + a[1] * a[1] + a[2] * a[2]; }
static inline float vec3_magnitude   (const float *a) { return sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]); }
static inline void  vec3_normalize   (float *a) { float m = 1.0f/vec3_magnitude(a); a[0] = a[0]*m; a[1] = a[1]*m; a[2] = a[2]*m; }
static inline void  vec3_lerp        (const float *a, const float *b, float t, float *result) { 
	result[0] = a[0] + (b[0] - a[0])*t;
	result[1] = a[1] + (b[1] - a[1])*t;
	result[2] = a[2] + (b[2] - a[2])*t;}

static inline void  vec2_lerp        (const float *a, const float *b, float t, float *result) {
	result[0] = a[0] + (b[0] - a[0])*t;
	result[1] = a[1] + (b[1] - a[1])*t;}

void gen_cube(skg_mesh_t *mesh, skg_buffer_t *v_buffer, skg_buffer_t *i_buffer, int32_t *faces) {
	skg_vert_t verts[24];
	uint32_t   inds [36];
	for (size_t i = 0; i < 24; i++) {
		float neg = (float)((i / 4) % 2 ? -1 : 1);
		int nx  = ((i+24) / 16) % 2;
		int ny  = (i / 8)       % 2;
		int nz  = (i / 16)      % 2;
		int u   = ((i+1) / 2)   % 2; // U: 0,1,1,0
		int v   = (i / 2)       % 2; // V: 0,0,1,1

		verts[i].uv[0] = (float)u;
		verts[i].uv[1] = (float)v;
		verts[i].norm[0] = nx * neg;
		verts[i].norm[1] = ny * neg;
		verts[i].norm[2] = nz * neg;
		verts[i].pos[0] = (nx ? neg : ny ? (u?-1:1)*neg : (u?1:-1)*neg);
		verts[i].pos[1] = (nx || nz ? (v?1:-1) : neg);
		verts[i].pos[2] = (nx ? (u?-1:1)*neg : ny ? (v?1:-1) : neg);
		verts[i].col.r = 255;
		verts[i].col.g = 255;
		verts[i].col.b = 255;
		verts[i].col.a = 255;
	}
	for (uint32_t i = 0; i < 6; i++) {
		inds[i*6+0] = i*4;
		inds[i*6+1] = i*4+1;
		inds[i*6+2] = i*4+2;

		inds[i*6+3] = i*4;
		inds[i*6+4] = i*4+2;
		inds[i*6+5] = i*4+3;
	}

	*v_buffer = skg_buffer_create(verts, sizeof(verts)/sizeof(skg_vert_t), sizeof(skg_vert_t), skg_buffer_type_vertex, skg_use_static);
	*i_buffer = skg_buffer_create(inds,  sizeof(inds )/sizeof(uint32_t  ), sizeof(uint32_t),   skg_buffer_type_index,  skg_use_static);
	*mesh     = skg_mesh_create  (v_buffer, i_buffer);
	*faces    = 36;
}

///////////////////////////////////////////

void mesh_gen_cube_vert(int i, float* size, float* pos, float* norm, float* uv) {
	float neg = (float)((i / 4) % 2 ? -1 : 1);
	int nx  = ((i+24) / 16) % 2;
	int ny  = (i / 8)       % 2;
	int nz  = (i / 16)      % 2;
	int u   = ((i+1) / 2)   % 2; // U: 0,1,1,0
	int v   = (i / 2)       % 2; // V: 0,0,1,1

	uv  [0] = (float)u; uv[1] = (float)v;
	norm[0] = nx*neg; norm[1] = ny*neg; norm[2] = nz*neg;
	pos [0] = size[0] * (nx ? neg : ny ? (u?-1:1)*neg : (u?1:-1)*neg);
	pos [1] = size[1] * (nx || nz ? (v?1:-1) : neg);
	pos [2] = size[2] * (nx ? (u?-1:1)*neg : ny ? (v?1:-1) : neg);
}

///////////////////////////////////////////

void gen_sphere(float diameter, int32_t subdivisions, skg_mesh_t *mesh, skg_buffer_t *v_buffer, skg_buffer_t *i_buffer, int32_t *faces) {
	uint32_t subd   = (uint32_t)subdivisions;

	subd = (subd<0?0:subd) + 2;

	int vert_count = 6*subd*subd;
	int ind_count  = 6*(subd-1)*(subd-1)*6;
	skg_vert_t *verts = (skg_vert_t*)malloc(sizeof(skg_vert_t) * vert_count);
	uint32_t   *inds  = (uint32_t  *)malloc(sizeof(uint32_t  ) * ind_count);

	float    size[3] = {1,1,1};
	float    radius = diameter / 2;
	int      ind    = 0;
	uint32_t offset = 0;
	for (uint32_t i = 0; i < 6*4; i+=4) {
		float p1[3], p2[3], p3[3], p4[3];
		float n1[3], n2[3], n3[3], n4[3];
		float u1[2], u2[2], u3[2], u4[2];

		mesh_gen_cube_vert(i,   size, p1, n1, u1);
		mesh_gen_cube_vert(i+1, size, p2, n2, u2);
		mesh_gen_cube_vert(i+2, size, p3, n3, u3);
		mesh_gen_cube_vert(i+3, size, p4, n4, u4);

		offset = (i/4) * (subd)*(subd);
		for (uint32_t y = 0; y < subd; y++) {
			float  py    = y / (float)(subd-1);
			uint32_t yOff  = offset + y * subd;
			uint32_t yOffN = offset + (y+1) * subd;

			float pl[3], pr[3];
			float ul[2], ur[2];
			vec3_lerp(p1, p4, py, pl);
			vec3_lerp(p2, p3, py, pr);
			vec2_lerp(u1, u4, py, ul);
			vec2_lerp(u2, u3, py, ur);

			for (uint32_t x = 0; x < subd; x++) {
				float px = x / (float)(subd-1);
				uint32_t  ptIndex = x + yOff;
				skg_vert_t *pt = &verts[ptIndex];

				vec3_lerp(pl, pr, px, pt->norm); 
				vec3_normalize(pt->norm);
				pt->pos[0] = pt->norm[0] * radius;
				pt->pos[1] = pt->norm[1] * radius;
				pt->pos[2] = pt->norm[2] * radius;
				vec2_lerp(ul, ur, px, pt->uv);
				pt->col = {255,255,255,255};

				if (y != subd-1 && x != subd-1) {

					inds[ind++] = (x  ) + yOff;
					inds[ind++] = (x+1) + yOff;
					inds[ind++] = (x+1) + yOffN;

					inds[ind++] = (x  ) + yOff;
					inds[ind++] = (x+1) + yOffN;
					inds[ind++] = (x  ) + yOffN;
				}
			}
		}
	}

	*v_buffer = skg_buffer_create(verts, vert_count, sizeof(skg_vert_t), skg_buffer_type_vertex, skg_use_static);
	*i_buffer = skg_buffer_create(inds,  ind_count,  sizeof(uint32_t),   skg_buffer_type_index,  skg_use_static);
	*mesh     = skg_mesh_create  (v_buffer, i_buffer);
	*faces    = ind_count;

	free(verts);
	free(inds);
}