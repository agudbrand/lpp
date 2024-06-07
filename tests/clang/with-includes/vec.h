
struct vec3
{
	union
	{
		float arr[3];
		struct { float x, y, z; };
	};
};


