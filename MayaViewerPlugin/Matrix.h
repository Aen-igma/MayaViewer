#pragma once


struct Mat4f {

	Mat4f():arr{
		1.f, 0.f, 0.f, 0.f,
		0.f, 1.f, 0.f, 0.f,
		0.f, 0.f, 1.f, 0.f,
		0.f, 0.f, 0.f, 1.f} {}

	union {
		float mat[4u][4u];
		float arr[16];
		struct {
			float 
				a00, a10, a20, a30,
				a01, a11, a21, a31,
				a02, a12, a22, a32,
				a03, a13, a23, a33;
		};
	};
};