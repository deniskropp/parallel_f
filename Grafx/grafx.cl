typedef unsigned int uint32_t;

struct vector2i
{
	int x;
	int y;
};

struct bounds
{
	struct vector2i tl;
	struct vector2i br;
};

struct color
{
	float r;
	float g;
	float b;
	float a;
};

enum flags {
	none = 0x00,
	opaque = 0x01
};

struct element
{
	struct bounds bounds;
	enum flags flags;
};

struct rect
{
	struct element base;

	struct color color;
};

struct triangle
{
	struct element base;

	struct color color;

	struct {
		struct vector2i v1;
		struct vector2i v2;
		struct vector2i v3;
	} coords;
};


enum type
{
	rect,
	triangle
};

struct instance
{
	enum type type;
	int index;
};


// Utility functions

uint32_t color_to_argb(__global struct color* color)
{
	int r = color->r * 255;
	int g = color->g * 255;
	int b = color->b * 255;
	int a = color->a * 255;

	return (a << 24) | (b << 16) | (g << 8) | r;
}


// Processing functions

bool process_rect(__global struct rect* rect, int x, int y)
{
	if (x >= rect->base.bounds.tl.x && x < rect->base.bounds.br.x &&
		y >= rect->base.bounds.tl.y && y < rect->base.bounds.br.y && (rect->base.flags & opaque))
		return true;
	
	return false;
}

bool process_triangle(__global struct triangle* triangle, int x, int y)
{
	return false;
}

bool process_instance(__global struct rect* rects,
					  __global struct triangle* tris,
					  __global struct instance* instances,
					  int index, int x, int y)
{
	switch (instances[index].type) {
		case rect:
			return process_rect(&rects[instances[index].index], x, y);
		case triangle:
			return process_triangle(&tris[instances[index].index], x, y);
	}

	return false;
}


// Rendering functions

void render_instance(__global struct rect* rects,
					 __global struct triangle* tris,
					 __global struct instance* instances,
					 int index, int x, int y,
					 uint32_t* output)
{
	__global struct rect* rect_;
	__global struct triangle* tri;
	
	switch (instances[index].type) {
		case rect:
			rect_ = &rects[instances[index].index];

			if (x >= rect_->base.bounds.tl.x && x < rect_->base.bounds.br.x &&
				y >= rect_->base.bounds.tl.y && y < rect_->base.bounds.br.y)
				*output = color_to_argb(&rect_->color);

			break;
		case triangle:
			break;
	}
}


// OpenCL Kernel Entry Points

__kernel 
void render(__global uint32_t* image,
			__global struct rect* rects,
			__global struct triangle* tris,
			__global struct instance* instances,
			int num_instances)
{
	int idx = get_global_id(0);
	int x = idx % 1000;
	int y = idx / 1000;

	uint32_t pixel = 0;

	int i;

	for (i=num_instances-1; i>=0; i--) {
		if (process_instance(rects, tris, instances, i, x, y))
			break;
	}

	for (; i<num_instances; i++)
		render_instance(rects, tris, instances, i, x, y, &pixel);

	image[idx] = pixel;
}
