

// OpenCL Kernel Entry Points

__kernel 
__attribute__((reqd_work_group_size(256, 1, 1)))
void CLTest1(__global uchar4* in, __global uchar4* out, int n)
{
	int idx = get_global_id(0);

	uchar4 tmp0, tmp1, tmp2, tmp3;

	if (idx < n)
	{
		// Read input from memory
		tmp0 = in[idx*4+0];
		tmp1 = in[idx*4+1];
		tmp2 = in[idx*4+2];
		tmp3 = in[idx*4+3];

		// Save results to memory
		out[idx*4+0] = tmp0;
		out[idx*4+1] = tmp1;
		out[idx*4+2] = tmp2;
		out[idx*4+3] = tmp3;
	}
}

__kernel 
__attribute__((reqd_work_group_size(256, 1, 1)))
void CLTest2(__global uchar4* in, __global uchar4* out, int n)
{
	int idx = get_global_id(0);

	uchar4 tmp0, tmp1, tmp2, tmp3;

	if (idx < n)
	{
		// Read input from memory
		tmp0 = in[idx*4+0];
		tmp1 = in[idx*4+1];
		tmp2 = in[idx*4+2];
		tmp3 = in[idx*4+3];

		for (int i=0; i<1024*64; i++) {
			tmp0 += tmp1 + tmp2 + tmp3;
			tmp1 += tmp0 + tmp2 + tmp3;
			tmp2 += tmp0 + tmp1 + tmp3;
			tmp3 += tmp0 + tmp1 + tmp2;
		}

		// Save results to memory
		out[idx*4+0] = tmp0;
		out[idx*4+1] = tmp1;
		out[idx*4+2] = tmp2;
		out[idx*4+3] = tmp3;
	}
}


struct object
{
	int x;
	int y;
	int seq;
};

__kernel 
//__attribute__((reqd_work_group_size(256, 1, 1)))
void RunObjects(__global struct object* obj, int n)
{
	int idx = get_global_id(0);

	if (idx < n) {
		if (++obj[idx].seq == 100)
			obj[idx].seq = 0;
	}
}
