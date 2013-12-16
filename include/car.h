#ifndef CAR_H
#define CAR_H

#include "lin_alg.h"

class Car {
public:
	union {
		struct {
			float pos[3];
			float direction;
			float wheel_rot;
			float susp_angle_roll;
			float velocity;
			float front_wheel_angle;
		} state;

		float data_serial[8];
	};

	// these are used to compute other variables at the server-side, no need to be passing them around
	struct {
		float front_wheel_tmpx;
		float F_centripetal;
		float susp_angle_fwd;

	} data_internal;

	vec4 position() const { return vec4(state.pos[0], state.pos[1], state.pos[2], 1.0); }
	
	Car() { memset(this, 0, sizeof(*this)); }
}; 


#endif
