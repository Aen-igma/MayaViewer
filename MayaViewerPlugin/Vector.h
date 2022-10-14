#pragma once


struct Vec4f {

	Vec4f(): x(0.f), y(0.f), z(0.f), w(0.f) {}

	union {
		struct {float x, y, z, w;};
		float arr[4];
	};
};

struct Vec3f {

	Vec3f(): x(0.f), y(0.f), z(0.f) {}

	union {
		struct {float x, y, z;};
		float arr[3];
	};
};

struct Vec2f {

	Vec2f(): x(0.f), y(0.f) {}

	union {
		struct {float x, y;};
		float arr[2];
	};
};