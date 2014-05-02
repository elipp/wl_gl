#ifndef OBB_H
#define OBB_H

#include "lin_alg.h"
#include <iostream>
#include <vector>
#include <sstream>
#include <stdint.h>
#include <limits>

enum { GJK_INCONCLUSIVE = -1, GJK_NOCOLLISION = 0, GJK_COLLISION = 1 };
enum { EPA_INCONCLUSIVE = -1, EPA_FAIL = 0, EPA_SUCCESS = 1 };


// TODO: get rid of this!
static inline std::string print_vec4(const vec4 &v) {
	std::ostringstream s;
	s << v;
	return s.str();
}

BEGIN_ALIGN16
struct OBB {
	vec4 A0, A1, A2;	// orthonormal, right-handed axes. 
						// Always updated to reflect the rotation of the host body.
	vec4 e;	// corresponding extents.
	vec4 C;	// center point
	void rotate(Quaternion &q) {
		// this sux, tbh :P
		const mat4 qm = q.toRotationMatrix();
		A0 = (qm*vec4(1.0, 0.0, 0.0, 0.0)).normalized();
		A1 = (qm*vec4(0.0, 1.0, 0.0, 0.0)).normalized();
		A2 = (qm*vec4(0.0, 0.0, 1.0, 0.0)).normalized();
	}
	OBB() {
		A0 = vec4(1.0, 0.0, 0.0, 0.0);
		A1 = vec4(0.0, 1.0, 0.0, 0.0);
		A2 = vec4(0.0, 0.0, 1.0, 0.0);
		C = vec4(0.0, 0.0, 0.0, 1.0);
	}

	OBB(const vec4 &extents) {
		A0 = vec4(1.0, 0.0, 0.0, 0.0);
		A1 = vec4(0.0, 1.0, 0.0, 0.0);
		A2 = vec4(0.0, 0.0, 1.0, 0.0);
		C = vec4(0.0, 0.0, 0.0, 1.0);
		e = extents;
	}
	void compute_box_vertices(vec4 *out8) const;
} END_ALIGN16;

int collision_test_SAT(const OBB &a, const OBB &b);

BEGIN_ALIGN16
struct mat4_doublet {
	mat4 m[2];
	mat4_doublet(const mat4 &m0, const mat4 &m1) { m[0] = m0; m[1] = m1; }
	mat4_doublet transposed_both() const {
		return mat4_doublet(m[0].transposed(), m[1].transposed());
	}
	mat4 &operator()(int n) { return m[n]; }
	vec4 column(int col) { return col > 3 ? m[1].column(col - 4) : m[0].column(col); }
	mat4_doublet() {};
} END_ALIGN16;

BEGIN_ALIGN16
struct simplex {
	vec4 points[4];
	int current_num_points;

	simplex() { 
		current_num_points = 0;
	}

	void assign(const vec4 &a) {
		current_num_points = 1;
		points[0] = a;
	}

	void assign(const vec4 &a, const vec4 &b) {
		current_num_points = 2;
		points[0] = b;
		points[1] = a;
	}
	void assign(const vec4 &a, const vec4 &b, const vec4 &c) {
		current_num_points = 3;
		points[0] = c;
		points[1] = b;
		points[2] = a;
	}
	void clear() { current_num_points = 0; }

	void add(const vec4 &v) {
		points[current_num_points] = v;
		++current_num_points;
	}
} END_ALIGN16;

BEGIN_ALIGN16
class GJKSession {
private:
	mat4_doublet VAm, VAm_T;
	mat4_doublet VBm, VBm_T;
	vec4 support(const vec4 &D);
	simplex current_simplex;
public:
	// don't like the interface though.
	GJKSession(const OBB &a, const OBB &b);
	int collision_test();
	int EPA_penetration(vec4 *outv);	
	
	simplex get_terminating_simplex() const { return current_simplex; }

	/* these three are for the visualization/debug program */
	int EPA_penetration_stepwise_record(vec4 *outv);

} END_ALIGN16;

template <typename T> 
struct aligned16_object {
	ALIGNED16(T object);
	char padding[16-(sizeof(T)%16)];
	aligned16_object(const T& t) : object(t) {};
	aligned16_object() {};
};

// use only for PODs plx
template <typename T> struct my_aligned16_vector {
	
#define TEST_CAPACITY 1024

	class iterator {
	public:
		aligned16_object<T> *ptr;		

		T &operator*() const {
			return ptr->object;
		}
		T* operator->() const {
			return &ptr->object;
		}

		iterator &operator++() {
			++ptr;
			return *this;
		}

		iterator &operator++(int i) {
			ptr += i;
			return *this;
		}

		bool operator==(const iterator &other) const {
			return (this->ptr == other.ptr);

		}

		iterator &operator=(const T &t) {
			ptr->object = t;
		}

		bool operator!=(const iterator &other) const {
			return (this->ptr != other.ptr);
		}

		int operator-(const iterator &other) const {
			return this->ptr - other.ptr;
		}

		iterator(aligned16_object<T> &t) {
			ptr = &t;			
		}
	};

	aligned16_object<T> memblock[TEST_CAPACITY];
	//T *memblock;
	int current_size;
	int capacity;

	int size() const { return current_size; }

	void reallocate(size_t new_capacity) {
	/*	fprintf(stderr, "my_aligned_vector::reallocate: reallocing to a new size of %lld\n", (long long)new_capacity);
		T *newblock = (T*)_aligned_malloc(new_capacity*sizeof(T), alignment);
		for (int i = 0; i < current_size; ++i) {
			newblock[i] = memblock[i];	// memcpy can cause all sorts of problemz; this should invoke a proper copy constructor and stuff	
		}
		_aligned_free(memblock);
		memblock = newblock;
		capacity = new_capacity;*/
	}

	my_aligned16_vector(size_t capacity_ = TEST_CAPACITY) {
		//memblock = (T*)_aligned_malloc(capacity_*sizeof(T), alignment);
		current_size = 0;
		capacity = capacity_;
	}

	/*
	~my_aligned_vector() {
		for (int i = 0; i < current_size; ++i) {
			memblock[i].T::~T();
		}
		//_aligned_free(memblock);
	}*/

	iterator push_back(const T& t) {
		if (current_size >= capacity) {
			reallocate(2*capacity);
		}
		memblock[current_size] = aligned16_object<T>(t);
		++current_size;
		return iterator(memblock[current_size - 1]);
	}

	iterator push_back(const iterator &it) {
		if (current_size >= capacity) {
			reallocate(2*capacity);
		}
		memblock[current_size] = *it.ptr;
		++current_size;
		return iterator(memblock[current_size - 1]);
	}

	void erase(int index) {
		if (index > current_size - 1) { fprintf(stderr, "my_aligned_vector::erase: index (%lld) > current_size - 1 (%lld)!\n", (long long)index, (long long)(current_size - 1)); }
		for (int i = index; i < current_size - 1; ++i) {
			memblock[i] = memblock[i+1];
		}
		--current_size;
	}

	int get_index(const T &t) {
		// the actual Ts are contained within the aligned+padded container, so a ptr-arithmetic trick is in order
		const aligned16_object<T> *ptr = (const aligned16_object<T> *)&t; // this is assuming the objects actually reside within the container
		return ptr - memblock;
	}

	T& operator[](size_t index) {
		return memblock[index].object;
	}

	iterator begin() {
		return iterator(memblock[0]);
	}

	iterator end() {
		return iterator(memblock[current_size]);
	}

	void append(T* from_beg, T* from_end) {

		int num_elements = (from_end - from_beg);
		if (current_size + num_elements >= capacity) {
			size_t diff = (current_size + num_elements) - capacity;
			reallocate(2*current_size);
		}
		
		for (int i = 0; i < num_elements; ++i) {
			memblock[current_size + i] = (aligned16_object<T>(*(from_beg + i)));
		}
		
		//fprintf(stderr, "my_aligned_vector::append: appending %d elements to position %d\n", (int)num_elements, current_size);
		current_size += num_elements;
		//fprintf(stderr, "current_size after append = %d\n", current_size);

	}

	void clear() {
		//*this = my_aligned_vector<T, alignment>(); // this ought to call an appropriate destructor for all elements
		current_size = 0;
	}
};

BEGIN_ALIGN16
struct hull_p {
	vec4 p;
	int refcount;
	hull_p(const vec4 &v) { p = v; refcount = 0; };
	hull_p() { refcount = 0; };

	DEFINE_ALIGNED_MALLOC_FREE_MEMBERS;
} END_ALIGN16;

BEGIN_ALIGN16
struct triangle_face {
	vec4 normal;
	hull_p *points[3];
	triangle_face *adjacents[3];	// index 0: edge p0->p1 neighbor, index 1: edge p1->p2 neighbor, index 2: edge p2->p0 neighbor
	float orthog_distance_from_origin;
	int origin_proj_within_triangle;
	int obsolete;

	triangle_face(my_aligned16_vector<hull_p> &point_base, int index0, int index1, int index2) {
		points[0] = &point_base[index0];
		points[1] = &point_base[index1];
		points[2] = &point_base[index2];
		
		++point_base[index0].refcount;
		++point_base[index1].refcount;
		++point_base[index2].refcount;
		
		adjacents[0] = NULL;
		adjacents[1] = NULL;
		adjacents[2] = NULL;
		
		// the implementation doesn't filter for duplicate point entries, so getting a 0-vector as a normal is indeed possible
		this->normal = cross(points[1]->p - points[0]->p, points[2]->p - points[0]->p);
		
		//if (normal.length3_squared() > 0.001) { normalize }
		// getting a NaN/-1.#IND is actually beneficial to us here; any comparison with a NaN returns false, so 
		// std::sort will sort invalid faces (ie ones with duplicate points -> area of zero) to the bottom of the heap
		this->normal.normalize();	
	
		orthog_distance_from_origin = dot3(this->normal, points[0]->p);	// any of the three points should do.
		origin_proj_within_triangle = contains_origin_proj();
		obsolete = 0;
	}
	triangle_face() {};

	bool is_visible_from(const vec4 &p);	// test whether the face is visible from the point p (a simple plane test)
	bool contains_point(const vec4 &p);
	bool contains_origin_proj();

	DEFINE_ALIGNED_MALLOC_FREE_MEMBERS;
} END_ALIGN16;

struct convex_hull {
	
	my_aligned16_vector<hull_p> points;	// ALL added points (including obsolete ones) are kept in this vector
	my_aligned16_vector<triangle_face> faces;	// similarly for faces
	std::vector<triangle_face*> active_faces;
	
	int add_point(const vec4 &p) { points.push_back(hull_p(p)); return points.size()-1; }
	int purge_triangles_visible_from_point(const vec4 &p);
	
	int add_face(int index0, int index1, int index2) {
		faces.push_back(triangle_face(points, index0, index1, index2));
		return faces.end() - faces.begin(); 
	}
	
	triangle_face *get_closest_valid() {
		triangle_face *best = NULL;

		auto iter = active_faces.begin();
		// find first face that actually contains the origin proj
		while (iter != active_faces.end()) {
			if ((*iter)->origin_proj_within_triangle) {
				best = *iter;
				break;
			}
			++iter;
		}

		while (iter != active_faces.end()) {
			if ((*iter)->origin_proj_within_triangle && 
				((*iter)->orthog_distance_from_origin < best->orthog_distance_from_origin)) {
				best = (*iter);
			}
			++iter;
		}
		return best;
	}

	bool has_dupe_in_active(const vec4 &v) { 
		const float margin = 0.01;
		for (auto &p : this->points) {
			if (p.refcount > 0) {
				if (roughly_equal(v, p.p, margin)) {
					// we're not adding duplicate points: 
					// massive issues (-1.#IND000 normal vectors, negative orthog distances etc etc) are inbound if that's allowed to happen
					return true;
				}
			}
		}
		return false;
	}
};


#endif
