#pragma once

#include <cstdint>
#include <vector>

class particle {
public:
	// Lifetime
	float delay;
	float lifetime;
	
	// Position
	float start_x, start_y, start_z;
	float x, y, z;
	
	// Size
	float start_sx, start_sy;
	float sx, sy;

	// Rotation
	float start_rotation;
	float roration;

	// Position affector
	// affector<float> ax, ay, az;
	
	// Size affector
	// affector<float> asx, asy;

	// Rotation affector
	// affector<float> arotation;
};

class particle_system {
public:
	void update(float dt) {
		_position += dt;

		for (auto&& part : _particles) {
			if (part.lifetime <= 0)
				continue;

			// Lifetime
			part.lifetime -= dt;

			// Position over time
			part.x += _velocity_x * dt;
			part.y += _velocity_y * dt;
			part.z += _velocity_z * dt;
		}
	}

private:
	// Lifetime
	float _duration;
	float _position;
	bool _looping;

	float _start_delay;
	float _start_lifetime;
	float _start_speed_x;
	float _start_speed_y;
	float _start_rotation;
	float _start_size_x;
	float _start_size_y;

	// Position over time
	float _velocity_x;
	float _velocity_y;
	float _velocity_z;

	std::vector<particle> _particles;

	// Emitter
	// emitter _emitter;
};