#include "physics/OBB.h"
#include <stdint.h>
#include <limits>
#include <algorithm>

inline vec4 triple_cross_1x2x1(const vec4 &a, const vec4 &b) {
	// use the identity (a x b) x c = -(c . b)a + (c . a)b (http://en.wikipedia.org/wiki/Vector_triple_product#Vector_triple_product)
	// let c = a:	    (a x b) x a = -(a . b)a + (a . a)b 
	// also				a x (b x c) = (a . c)b - (a . b)c
	// let c = a:			a x (b x a) = (a . a)b - (a . b)a
	
	// ^ that's all good, but it turns out it's faster to just do a cross(cross(a,b),a) (dot products are so slow :()
	
	// return ((-dot3(a,b))*a) + (dot3(a,a)*b);	// 
	// return (dot3(a, a)*b) - (dot3(a,b)*a);

	return cross(cross(a,b),a);
}

// perform casts from __m128i -> __m128 -> __m128i without performing any kind of conversion (reinterpret)
// http://stackoverflow.com/questions/13631951/bitwise-cast-from-m128-to-m128i-on-msvc

#ifdef _WIN32
#define m128i_CAST(m128_var) (_mm_castps_si128(m128_var))	// intrinsics are required on windows :P
#define m128_CAST(m128i_var) (_mm_castsi128_ps(m128i_var))
#elif __linux__
#define m128i_CAST(m128_var) ((__m128i)(m128_var))
#define m128_CAST(m128i_var) ((__m128)(m128i_var))
#endif

inline __m128i _expand_int_mask(int mask) {
	static const __m128i mulShiftImm = _mm_set_epi32(0x10000000, 0x20000000, 0x40000000, 0x80000000);
	__m128i s = _mm_set1_epi16(mask);
	s = _mm_mullo_epi16(s, mulShiftImm);
	s = _mm_srai_epi32(s, 31);	// now we have extended the four-bit mask into a full xmm register
	return s;
}

inline __m128 __mm_blend_ps_emul(__m128 a, __m128 b, int mask) {
	__m128i s = _expand_int_mask(mask);
	__m128i ra = _mm_and_si128(m128i_CAST(a), s);	// zero out the unwanted stuff from a
	__m128i rb = _mm_andnot_si128(s, m128i_CAST(b));	// zero out unwanted stuff from b
	__m128i reti = _mm_or_si128(ra, rb);	// combine them with or
	__m128 ret = m128_CAST(reti);
	return ret;

}

inline __m128i __mm_blend_epi32_emul(__m128i a, __m128i b, int mask) {
	__m128 tmp = __mm_blend_ps_emul(m128_CAST(a), m128_CAST(b), mask);
	return m128i_CAST(tmp);
}

inline int find_hi_index_ps(__m128i indices, __m128 floats) {

	ALIGNED16(int ind[4]);
	_mm_store_si128((__m128i*)&ind[0], indices);
	int highest_i = ind[0];

	ALIGNED16(float f[4]);
	_mm_store_ps(f, floats);
	float highest_f = f[0];

	for (int i = 1; i < 4; ++i) {
		if (f[i] > highest_f) {
			highest_i = ind[i];
			highest_f = f[i];
		}
	}
	return highest_i;

}

static int find_largest_float_index_8f(__m128 *floats_aligned16) {

	__m128i cur_indices = _mm_set_epi32(3, 2, 1, 0);
	__m128i highest_indices = cur_indices;

	__m128 cur = floats_aligned16[0];
	static const __m128i increment4 = _mm_set1_epi32(4);
	__m128 cur_highest = cur;
	
	cur = floats_aligned16[1];
	__m128 cmp = _mm_cmpge_ps(cur, cur_highest);
	int blendmask = _mm_movemask_ps(cmp);
	cur_indices = _mm_add_epi32(cur_indices, increment4);

	// pick the floats that were bigger than the corresponding float from the last chunk
	cur_highest = __mm_blend_ps_emul(cur, cur_highest, blendmask);
	// use same mask for indices :P
	highest_indices = __mm_blend_epi32_emul(cur_indices, highest_indices, blendmask);

	return find_hi_index_ps(highest_indices, cur_highest);

}

static int find_farthest_in_direction(mat4_doublet &point_chunks_transposed, const vec4 &D) {

	__m128 dps[2];
	dps[0] = dot3x4_notranspose(point_chunks_transposed(0), D);
	dps[1] = dot3x4_notranspose(point_chunks_transposed(1), D);
	
	int index = find_largest_float_index_8f(dps);

	return index;
}

// THIS IS REALLY BROKEN ATM.
int collision_test_SAT(const OBB &a, const OBB &b) {

	// test for collision between two OBBs using the Separating Axis Theorem (SAT).
	// http://www.geometrictools.com/Documentation/DynamicCollisionDetection.pdf
	
	// assume axes reflect the orientation of the actual body (ie. have already been multiplied by the orientation quaternion)

	// this is an awful lot of initialization code, considering that the function could actually terminate on the first test :D
	// actual performance gains are observed maybe in the worst case (test #15)
	mat4 A(a.A0, a.A1, a.A2, vec4::zero4);
	mat4 A_T = A.transposed();
	float_arr_vec4 a_e(a.e);

	mat4 B(b.A0, b.A1, b.A2, vec4::zero4);
	mat4 B_T = B.transposed();
	float_arr_vec4 b_e(b.e);

	mat4 C = A_T * B;	// get c_ij coefficients
	float_arr_mat4 C_f(C);

	mat4 absC = abs(C);
	mat4 absC_T = absC.transposed();
	float_arr_mat4 absC_T_f(absC_T);

	vec4 D = b.C - a.C;	// vector between the two center points
	D.assign(V::w, 0.0);

	vec4 A_T_D = A_T*D;
	float_arr_vec4 dp_Ai_D(A_T_D); // Ai · D
	float_arr_vec4 abs_dp_Ai_D(abs(A_T_D)); // |Ai · D|
	float_arr_vec4 abs_dp_Bi_D(abs(B_T*D)); // |Bi · D|
	// start testing for each of the 15 axes (R > R0 + R1). Reference PDF, Table 1 ^^

	float R, R0, R1;	// hopefully the compiler will optimize these away :P

	// L = Ai
	R0 = a_e(0);
	R1 = dot3(absC_T.column(0), b.e);	
	R = abs_dp_Ai_D(0);
	if (R > R0 + R1) { return 1; }

	R0 = a_e(1);
	R1 = dot3(absC_T.column(1), b.e);	
	R = abs_dp_Ai_D(1);
	if (R > R0 + R1) { return 1; }

	R0 = a_e(2);
	R1 = dot3(absC_T.column(2), b.e);
	R = abs_dp_Ai_D(2);
	if (R > R0 + R1) { return 1; }

	// L = Bi
	R0 = dot3(absC.column(0), a.e);
	R1 = b_e(0);
	R = abs_dp_Bi_D(0);
	if (R > R0 + R1) { return 1; }

	R0 = dot3(absC.column(1), a.e);
	R1 = b_e(1);
	R = abs_dp_Bi_D(1);
	if (R > R0 + R1) { return 1; }

	R0 = dot3(absC.column(2), a.e);
	R1 = b_e(2);
	R = abs_dp_Bi_D(2);
	if (R > R0 + R1) { return 1; }

	// various cross products
	
	// L = A0 x B0
	R0 = a_e(1)*absC_T_f(2, 0) + a_e(2)*absC_T_f(1,0);
	R1 = b_e(1)*absC_T_f(0, 2) + b_e(2)*absC_T_f(0,1);
	R = fabs(C_f(0,1)*dp_Ai_D(2) - C_f(0,2)*dp_Ai_D(1));
	if (R > R0 + R1) { return 1; }

	// L = A0 x B1
	R0 = a_e(1)*absC_T_f(2,1) + a_e(2)*absC_T_f(1,1);
	R1 = b_e(1)*absC_T_f(0,2) + b_e(2)*absC_T_f(0,0);
	R = fabs(C_f(1,1)*dp_Ai_D(2) - C_f(1,2)*dp_Ai_D(1));
	if (R > R0 + R1) { return 1; }

	// L = A0 x B2
	R0 = a_e(1)*absC_T_f(2,0) + a_e(2)*absC_T_f(1,0);
	R1 = b_e(1)*absC_T_f(0,1) + b_e(2)*absC_T_f(0,0);
	R = fabs(C_f(2,1)*dp_Ai_D(2) - C_f(2,2)*dp_Ai_D(1));
	if (R > R0 + R1) {}
	
	// L = A1 x B0
	R0 = a_e(0)*absC_T_f(2,0) + a_e(2)*absC_T_f(0,0);
	R1 = b_e(1)*absC_T_f(1,2) + b_e(2)*absC_T_f(1,1);
	R = fabs(C_f(0,2)*dp_Ai_D(0) - C_f(0,0)*dp_Ai_D(2));
	if (R > R0 + R1) { return 1; }

	// L = A1 x B1
	R0 = a_e(0)*absC_T_f(2,1) + a_e(2)*absC_T_f(0,1);
	R1 = b_e(0)*absC_T_f(1,2) + b_e(2)*absC_T_f(1,0);
	R = fabs(C_f(1,2)*dp_Ai_D(0) - C_f(0,0)*dp_Ai_D(2));
	if (R > R0 + R1) { return 1; }

	// L = A1 x B2
	R0 = a_e(0)*absC_T_f(2,2) + a_e(2)*absC_T_f(0,2);
	R1 = b_e(0)*absC_T_f(1,1) + b_e(1)*absC_T_f(1,0);
	R = fabs(C_f(2,2)*dp_Ai_D(0) - C_f(2,0)*dp_Ai_D(2));
	if (R > R0 + R1) { return 1; }
	
	// L = A2 x B0
	R0 = a_e(0)*absC_T_f(1,0) + a_e(1)*absC_T_f(0,0);
	R1 = b_e(1)*absC_T_f(2,2) + b_e(2)*absC_T_f(2,1);
	R = fabs(C_f(0,0)*dp_Ai_D(1) - C_f(0,1)*dp_Ai_D(0));
	if (R > R0 + R1) { return 1; }

	// L = A2 x B1
	R0 = a_e(0)*absC_T_f(1,1) + a_e(1)*absC_T_f(0,1);
	R1 = b_e(0)*absC_T_f(2,2) + b_e(2)*absC_T_f(2,0);
	R = fabs(C_f(1,0)*dp_Ai_D(1) - C_f(1,1)*dp_Ai_D(0));
	if (R > R0 + R1) { return 1; }

	// L = A2 x B2
	R0 = a_e(0)*absC_T_f(1,2) + a_e(1)*absC_T_f(0,2);
	R1 = b_e(0)*absC_T_f(2,1) + b_e(1)*absC_T_f(2,0);
	R = fabs(C_f(2,0)*dp_Ai_D(1) - C_f(2,1)*dp_Ai_D(0));
	if (R > R0 + R1) { return 1; }

	return 0;

} 

// GJK (Gilbert-Johnson-Keerthi). References:
// http://mollyrocket.com/849 !!,
// http://www.codezealot.org/archives/88.

static int find_max_dp_index(const float_arr_vec4 &p1, const float_arr_vec4 &p2) {
	
	int max_index_p1 = 0;
	int max_index_p2 = 0;
	float current_max_dp1 = p1(0);
	float current_max_dp2 = p2(0);

	for (int i = 1; i < 4; ++i) {
		float dp1 = p1(i);
		if (dp1 > current_max_dp1) {
			max_index_p1 = i;
			current_max_dp1 = dp1;
		}

		float dp2 = p2(i);
		if (dp2 > current_max_dp2) {
			max_index_p2 = i;
			current_max_dp2 = dp2;
		}
	}
	return p1(max_index_p1) > p2(max_index_p2) ? max_index_p1 : max_index_p2 + 4;
}

vec4 GJKSession::support(const vec4 &D) {
	// returns a point within the Minkowski difference of the two boxes.
	// Basically, we find the vertex that is farthest away in the direction D among
	// the vertices of box A (VA), and the one farthest away in direction -D among
	// the vertices of box B (VB).
	
	// the whole float_arr bullshit is because we're calculating 4 dot products at once (SSE), in two parts.

	/*float_arr_vec4 dpVA1(VAm_T(0)*D);	// now we have the first four dot products in the vector
	float_arr_vec4 dpVA2(VAm_T(1)*D);	// and the last four in another

	// find point with maximum dot product
	vec4 max_A = VAm.column(find_max_dp_index(dpVA1, dpVA2));
	
	const vec4 neg_D = -D;

	float_arr_vec4 dpVB1(VBm_T(0)*neg_D);	
	float_arr_vec4 dpVB2(VBm_T(1)*neg_D);	

	vec4 max_B = VBm.column(find_max_dp_index(dpVB1, dpVB2));*/

	vec4 max_A = VAm.column(find_farthest_in_direction(VAm_T, D));
	vec4 max_B = VBm.column(find_farthest_in_direction(VBm_T, -D));
	return max_A - max_B;	// minkowski difference, or negative addition
}

#define SAMEDIRECTION(va, vb) ((dot3((va), (vb))) > 0)
#define POINTS_TOWARDS_ORIGIN(va) (SAMEDIRECTION(va, AO))

typedef bool (*simplexfunc_t)(simplex*, vec4*);

// THE PURPOSE OF A "SIMPLEXFUNC" IS TO
// a) FIND THE FEATURE OF THE CURRENT SIMPLEX THAT'S CLOSEST TO THE ORIGIN 
// (EDGE, TRIANGLE FACE), AND SET THAT AS THE NEW SIMPLEX (WITH APPROPRIATE WINDING)
// b) UPDATE SEARCH DIRECTION ACCORDINGLY

static bool null_simplexfunc(simplex *s, vec4 *dir) { return false; }	
static bool line_simplexfunc(simplex *s, vec4 *dir) {
	const vec4 AO = -s->points[1];		// -A
	const vec4 AB = s->points[0] + AO;	// essentially B - A
	*dir = triple_cross_1x2x1(AB, AO);	// use this edge as next simplex
	
	// the conclusion of the discussion in https://mollyrocket.com/forums/viewtopic.php?t=271&postdays=0&postorder=asc&start=15
    // was that in the line case, the new point A cannot be the closest simplex feature, so we'll just use the edge AB, and search
	// in a direction that's perpendicular to the edge and towards the origin

	return false;
}

static bool triangle_simplexfunc(simplex *s, vec4 *dir) {
		// gets a tad more complicated here :P
		const vec4 A = s->points[2];
		const vec4 B = s->points[1];
		const vec4 C = s->points[0];

		const vec4 AO = -A;
		const vec4 AB = B-A;
		const vec4 AC = C-A;

		const vec4 ABC = cross(AB, AC);	// triangle normal
		
		// edge normals, pointing outwards from the triangle (on the triangle plane)
		const vec4 ABCxAC = cross(ABC, AC);	
		const vec4 ABxABC = cross(AB, ABC);

		// again, using the same principle as in the line case, the voronoi region of the new point A can be ruled out beforehand

		if (POINTS_TOWARDS_ORIGIN(ABCxAC)) {
			// then use edge AC
			s->assign(A, C);
			*dir = triple_cross_1x2x1(AC, AO);	// a direction perpendicular to the edge, and pointing towards the origin
		}
		else if (POINTS_TOWARDS_ORIGIN(ABxABC)) {
			// use edge AB
			s->assign(A, B);
			*dir = triple_cross_1x2x1(AB, AO);
		}
		else if (POINTS_TOWARDS_ORIGIN(ABC)) {
			// the origin lies within the triangle area, either "above" or "below" the plane
			*dir = ABC;	
		}
		else {
			// a permutation of the points B & C is in order to give consistent 
			// triangle winding (-> cross products) in the tetrahedral simplex processing phase
			s->assign(A, C, B);
			*dir = -ABC;
		}

		return false;

}

static bool tetrahedron_simplexfunc(simplex *s, vec4 *dir) {
	const vec4 A = s->points[3];
	const vec4 B = s->points[2];
	const vec4 C = s->points[1];
	const vec4 D = s->points[0];

	// redundant, but easy to read
	
	const vec4 AO = -A;
	const vec4 AB = B-A;
	const vec4 AC = C-A;
	const vec4 AD = D-A;

	// OUTWARD-FACING (outward from the tetrahedron) triangle normals.
	const vec4 ABC = cross(AB, AC);
	const vec4 ACD = cross(AC, AD);	
	const vec4 ADB = cross(AD, AB);

	// taken from casey's implementation at https://mollyrocket.com/forums/viewtopic.php?t=245&postdays=0&postorder=asc&start=41
	uint32_t code = 0;

	// this could be reimplemented using _mm_cmpgt_ps -> _mm_movemask_ps
	if (POINTS_TOWARDS_ORIGIN(ABC)) {
		code |= 0x01;
	}

	if (POINTS_TOWARDS_ORIGIN(ACD)) {
		code |= 0x02;
	}	

	if (POINTS_TOWARDS_ORIGIN(ADB)) {
		code |= 0x04;
	}

	// the tetrahedron winding tells us there's no need to test for triangle BCD, since
	// that dot product is bound to be negative

	switch(code) {
	case 0:
		// the origin was enclosed by the tetrahedron
		return true;
		break;
	case 1:
		// only in front of triangle ABC
		s->assign(A, B, C);
		return triangle_simplexfunc(s, dir); // because we still need to figure out which feature of the triangle simplex is closest to the origin
		break;
	case 2:
		// only in front of triangle ACD
		s->assign(A, C, D);
		return triangle_simplexfunc(s, dir);
		break;
	case 3: {
		// in front of both ABC and ACD -> differentiate the closest feature of all of the involved points, edges, or faces
		if (POINTS_TOWARDS_ORIGIN(cross(ABC, AC))) {
			if (POINTS_TOWARDS_ORIGIN(cross(AC, ACD))) {
				// there's actually another plane test after this in Casey's implementation, but
				// as said a million times already, the point A can't be closest to the origin
				//if (SAMEDIRECTION(AC, AO)) { // USE [A,C] } else { use [A] } // <- all of that is unnecessary
				s->assign(A, C);
				*dir = triple_cross_1x2x1(AC, AO);
	
			}
			else if (POINTS_TOWARDS_ORIGIN(cross(ACD, AD))) {
				s->assign(A, D);	// edge AD
				*dir = triple_cross_1x2x1(AD, AO);
			}
			else {
				s->assign(A, C, D);	// the triangle A,C,D.
				*dir = ACD;	
			}
		}
		else if (POINTS_TOWARDS_ORIGIN(cross(AB, ABC))) {
			// again, differentiating between whether the edge AB is closer to the origin than the point A is unnecessary, so just skip
			// if (SAMEDIRECTION(AB, AO)) {
				s->assign(A, B);
				*dir = triple_cross_1x2x1(AB, AO);
			//}
			// else { s->assign(A); }
		}
		else {
			s->assign(A, B, C);
			*dir = ABC;
		}

		break;
		}
	case 4:
		// only in front of ADB
		s->assign(A, D, B);
		return triangle_simplexfunc(s, dir);
		break;
	case 5: {
		// in front of ABC & ADB
		if (POINTS_TOWARDS_ORIGIN(cross(ADB, AB))) {
			if (POINTS_TOWARDS_ORIGIN(cross(AB, ABC))) {
				s->assign(A, B);
				*dir = triple_cross_1x2x1(AB, AO);
			}
			else if (POINTS_TOWARDS_ORIGIN(cross(ABC, AC))) {
				s->assign(A, C);
				*dir = triple_cross_1x2x1(AC, AO);
			}
			else {
				s->assign(A, B, C);
				*dir = ABC;
			}
		}
		else {
			if (POINTS_TOWARDS_ORIGIN(cross(AD, ADB))) {
				// there would be a gratuitous check for SAMEDIRECTION(AD, AO)
				s->assign(A, D);
				*dir = triple_cross_1x2x1(AD, AO);
			}
			else {
				s->assign(A, D, B);
				*dir = ADB;
			}
		}
		
		break;
	}
	case 6: {
		// in front of ACD & ADB
		if (POINTS_TOWARDS_ORIGIN(cross(ACD, AD))) {
			if (POINTS_TOWARDS_ORIGIN(cross(AD, ADB))) {
				// if SAMEDIRECTION(AD, AO), not needed!
				s->assign(A, D);
				*dir = triple_cross_1x2x1(AD, AO);
			}
			else if (POINTS_TOWARDS_ORIGIN(cross(ADB, AB))) {
				s->assign(A, B);
				*dir = triple_cross_1x2x1(AB, AO);
			}
			else {
				s->assign(A, D, B);
				*dir = ADB;
			}
		}
		else if (POINTS_TOWARDS_ORIGIN(cross(AC, ACD))) {
			// if SAMEDIRECTION(AC, AO), not needed!
			s->assign(A, C);
			*dir = triple_cross_1x2x1(AC, AO);
		}
		else {
			s->assign(A, C, D);
			*dir = ACD;
		}
		break;
	}
	case 7:
		// in front of all of them (which would imply that the origin is in the voronoi region of the newly added point A -> impossible)
		break;
	default:
		// wtf?
		break;
	}
	return false;
	
}
// jump table :P
static const simplexfunc_t simplexfuncs[5] = { 
	null_simplexfunc,	
	null_simplexfunc,
	line_simplexfunc,
	triangle_simplexfunc,
	tetrahedron_simplexfunc
};



static bool do_simplex(simplex *s, vec4 *D) {
	//std::cerr << "calling simplexfuncs[" << simplex_current_num_points << "] with dir = " << *D << ".\n";
	return simplexfuncs[s->current_num_points](s, D);
}

void OBB::compute_box_vertices(vec4 *vertices_out) const {
	const float_arr_vec4 extents(e);
	const vec4 precomp_val_A0 = 2*extents(0)*A0;
	const vec4 precomp_val_A1 = 2*extents(1)*A1;
	const vec4 precomp_val_A2 = 2*extents(2)*A2;

	vertices_out[0] = C + extents(0)*A0 + extents(1)*A1 + extents(2)*A2;
	vertices_out[1] = vertices_out[0] - precomp_val_A2;
	vertices_out[2] = vertices_out[1] - precomp_val_A0;
	vertices_out[3] = vertices_out[2] + precomp_val_A2;

	vertices_out[4] = vertices_out[0] - precomp_val_A1;
	vertices_out[5] = vertices_out[1] - precomp_val_A1;
	vertices_out[6] = vertices_out[2] - precomp_val_A1;
	vertices_out[7] = vertices_out[3] - precomp_val_A1;
}

GJKSession::GJKSession(const OBB &box_A, const OBB &box_B) {

	vec4 VA[8];
	box_A.compute_box_vertices(VA);

	VAm = mat4_doublet(mat4(VA[0], VA[1], VA[2], VA[3]), 
					   mat4(VA[4], VA[5], VA[6], VA[7]));
	
	VAm_T = VAm.transposed_both();

	vec4 VB[8];
	box_B.compute_box_vertices(VB);

	VBm = mat4_doublet(mat4(VB[0], VB[1], VB[2], VB[3]), 
				   mat4(VB[4], VB[5], VB[6], VB[7]));
	
	VBm_T = VBm.transposed_both();


}

int GJKSession::collision_test() {
	
	vec4 D(0.0, 1.0, 0.0, 0.0);	// initial direction (could be more "educated")
	
	// find farthest point (vertex) in the direction D for box a, and in -D for box b (to maximize volume/area for quicker convergence)
	vec4 S = support(D);
	D = -S;

	current_simplex.assign(S);

	// perhaps add a hard limit to the number of iterations to avoid infinite looping
#define ITERATIONS_MAX 20
	int i = 0;
	while (i < ITERATIONS_MAX) {
		D.assign(V::w, 0);
		D.normalize();
		vec4 A = support(D);
		if (dot3(A, D) < 0) {
			// we can conclude that there can be no intersection between the two shapes (boxes)
			return GJK_NOCOLLISION;
		}
		current_simplex.add(A);
		if (do_simplex(&this->current_simplex, &D)) { // updates our simplex and the search direction in a way that allows us to close in on the origin efficiently
			return GJK_COLLISION;
		}
		++i;
	}
	return GJK_INCONCLUSIVE;

}

inline int find_hi_index_float4(float *f4) {
	int cur_hi_index = 0;
	float cur_hi_float = f4[0];

	for (int i = 1; i < 4; ++i) {
		float cur_float = f4[i];
		if (cur_float > cur_hi_float) {
			cur_hi_index = i;
			cur_hi_float = cur_float;
		}
	}
	return cur_hi_index;
}

bool triangle_face::is_visible_from(const vec4 &p) {
	static const float margin = 0.01;
	return (dot3((p - this->points[0]->p), this->normal) > margin); // the result should be same (similar) for this->points[1]-> & this->points[2]->p
}

bool triangle_face::contains_point(const vec4 &p) {
	// assuming ccw winding
	// counting on some heavy compiler optimizations here :D
	const vec4 A = this->points[0]->p;
	const vec4 B = this->points[1]->p;
	const vec4 C = this->points[2]->p;

	const vec4 AB = B - A;
	const vec4 AC = C - A;
	const vec4 BC = C - B;

	// outward-pointing in-plane edge normals
	const vec4 ABxABC = cross(AB, this->normal);
	const vec4 ABCxAC = cross(this->normal, AC);	
	const vec4 BCxABC = cross(BC, this->normal);

	float d1 = dot3(p - A, ABxABC);
	float d2 = dot3(p - A, ABCxAC);
	float d3 = dot3(p - B, BCxABC);

	return (d1 < 0) && (d2 < 0) && (d3 < 0);

}

bool triangle_face::contains_origin_proj() {
	const vec4 A = this->points[0]->p;
	const vec4 B = this->points[1]->p;
	const vec4 C = this->points[2]->p;

	const vec4 AB = B - A;
	const vec4 AC = C - A;
	const vec4 BC = C - B;

	// outward-pointing in-plane edge normals
	const vec4 ABxABC = cross(AB, this->normal);
	const vec4 ABCxAC = cross(this->normal, AC);	
	const vec4 BCxABC = cross(BC, this->normal);

	return (dot3(A, ABxABC) > 0) && (dot3(A, ABCxAC) > 0) && (dot3(B, BCxABC) > 0);
}


int convex_hull::purge_triangles_visible_from_point(const vec4 &p) {

	int prev_size = active_faces.size();

	active_faces.erase(
		std::remove_if(active_faces.begin(), active_faces.end(), 
		[&p](triangle_face* f) { 
			if (f->is_visible_from(p)) {
				f->obsolete = 1;
				--f->points[0]->refcount;
				--f->points[1]->refcount;
				--f->points[2]->refcount;
				return true;
			}
			else {
				f->obsolete = 0;
				return false;
			}

		}), active_faces.end());
	
	return prev_size - (int)active_faces.size();
		
}


int GJKSession::EPA_penetration(vec4 *outv) {
	// start with the final simplex returned by GJK (tetrahedron)
	
	convex_hull hull;

	hull.add_point(current_simplex.points[3]);
	hull.add_point(current_simplex.points[2]);
	hull.add_point(current_simplex.points[1]);
	hull.add_point(current_simplex.points[0]);
	
	hull.add_face(0, 1, 2);	// ABC
	hull.add_face(0, 2, 3);	// ACD
	hull.add_face(0, 3, 1);	// ADB
	hull.add_face(1, 3, 2);	// BDC

	// manually specifying initial adjacency information. See struct triangle_face definition @ OBB.h
	hull.faces[0].adjacents[0] = &hull.faces[2];
	hull.faces[0].adjacents[1] = &hull.faces[3];
	hull.faces[0].adjacents[2] = &hull.faces[1];

	hull.faces[1].adjacents[0] = &hull.faces[0];
	hull.faces[1].adjacents[1] = &hull.faces[3];
	hull.faces[1].adjacents[2] = &hull.faces[2];

	hull.faces[2].adjacents[0] = &hull.faces[1];
	hull.faces[2].adjacents[1] = &hull.faces[3];
	hull.faces[2].adjacents[2] = &hull.faces[0];

	hull.faces[3].adjacents[0] = &hull.faces[2];	
	hull.faces[3].adjacents[1] = &hull.faces[1];
	hull.faces[3].adjacents[2] = &hull.faces[0];


	hull.active_faces.push_back(&hull.faces[0]);
	hull.active_faces.push_back(&hull.faces[1]);
	hull.active_faces.push_back(&hull.faces[2]);
	hull.active_faces.push_back(&hull.faces[3]);

	vec4 new_p;
	float terminating_margin = std::numeric_limits<float>::max();

	const int EPA_MAX_ITERATIONS = 32;

	for (int i = 0; i < EPA_MAX_ITERATIONS; ++i) {

		triangle_face *best = hull.get_closest_valid();
		if (!best) {
			fprintf(stderr, "EPA: WARNING! A best triangle face couldn't be determined. Returning EPA_FAIL.\n");
			return EPA_FAIL;
		}
		vec4 search_direction = best->normal;

		new_p = support(search_direction); // the search_direction vec4 (ie. best->normal) is normalized

		if (hull.has_dupe_in_active(new_p)) {	
			fprintf(stderr, "EPA: WARNING! The new point to be added was dangerously close to another (is indicative of convergence)\n");
			vec4 penetration_depth = best->orthog_distance_from_origin*best->normal;
			*outv = penetration_depth;	// we have converged to the penetration depth.
			return EPA_SUCCESS;
		}

		float new_margin = dot3(search_direction, new_p);

		if (fabs(new_margin - terminating_margin) < 0.000001) {
			// the algorithm has converged
			vec4 penetration_depth = best->orthog_distance_from_origin * best->normal;
			*outv = penetration_depth;
			fprintf(stderr, "EPA_pen: returning EPA_SUCCESS!\n\n");
			fprintf(stderr, "penetration depth = %s\n", print_vec4(best->normal*best->orthog_distance_from_origin).c_str());
			return EPA_SUCCESS;
		}
		terminating_margin = new_margin;	

		// else proceed to the convex hull computation part
		int num_obsolete = hull.purge_triangles_visible_from_point(new_p);
	
		if (num_obsolete < 1) {
			// the point to be added was enclosed by the shape (or already was included in the convex hull)
			// this can easily happen with two discrete (polygonal) shapes
			fprintf(stderr, "WARNING: nothing to delete (the point-to-be-added was enclosed by the shape!)\n");
			// actually, this implies EPA_SUCCESS, since the next iteration will be identical to 
			// this one (same search direction->same support point->same margin->termination)	
			vec4 penetration_depth = best->orthog_distance_from_origin * best->normal;
			*outv = penetration_depth;
			return EPA_SUCCESS;
		}

		int new_index = hull.add_point(new_p);

		std::vector<triangle_face*> new_faces;
		
		// figure out the horizon line loop, add the new faces to the master list
		for (auto &face : hull.active_faces) { // obsolete faces have already been removed
			int edge_mask = 0;
			for (int i = 0; i < 3; ++i) {
				if (face->adjacents[i]->obsolete != 0) {
					int v_index0 = face->points[i%3] - &hull.points[0];
					int v_index1 = face->points[(i+1)%3] - &hull.points[0];
					auto p = hull.faces.push_back(triangle_face(hull.points, v_index1, v_index0, new_index));	// <- note, permutation (1, 0, n) to preserve winding
					
					p->adjacents[0] = face; // the shared two points comprise edge p0->p1 of the new face
					face->adjacents[i] = &(*p);
					new_faces.push_back(&(*p));
				}
			}
			
		}

		// figure out adjacency status for the new faces

		// this part could use some serious commentary.
		for (auto &face_a : new_faces) {
			// adjacents[0] is never NULL, see above loop. adjacents[2] are on the other hand covered by the loop (edges are resolved reciprocally for adj triangles)
				for (auto &face_b : new_faces) { // i'd still say roughly O(n) despite double looping
					if (face_a == face_b) { continue; }
					// adjacent new_faces will always share an edge involving the new_index point (always at index 2). This can easily be verified geometrically.
					// the edge we're checking against consists of pa_1 & pa_2 (there's a strong guarantee that pa_2 == pb_2, ie. the newly added point, so no need to check for that).
					// given ccw winding and the fact that the new faces all share the point new_p, stored at new_index (found at index 2 of all faces),
					// the only case where the triangles share an edge is where pa_1 (the point in face_a "counter-clockwise right before the shared point") 
					// equals pb_0 (the point "counter-clockwise right AFTER the shared point")
					else if (face_a->points[1] == face_b->points[0]) {
							face_a->adjacents[1] = face_b;
							face_b->adjacents[2] = face_a;
					}
				}

		}

		hull.active_faces.insert(hull.active_faces.end(), new_faces.begin(), new_faces.end());


	}
		
	return EPA_INCONCLUSIVE;
}
	

