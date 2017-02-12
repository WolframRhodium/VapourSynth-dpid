// Copyright (c) 2016 Nicolas Weber and Sandra C. Amend / GCC / TU-Darmstadt. All rights reserved. 
// Use of this source code is governed by the BSD 3-Clause license that can be
// found in the LICENSE file.
#define _USE_MATH_DEFINES 
#include <math.h>
#include <iostream>
#include <cstdint>
#include <cuda_runtime.h>
#include <cuda_runtime_api.h>
#include <vector_types.h>
#include <vector_functions.h>

#define THREADS 128
#define WSIZE 32
#define TSIZE (THREADS / WSIZE)

#define TX threadIdx.x
#define PX (blockIdx.x * TSIZE + (TX / WSIZE))
#define PY blockIdx.y

#define WTHREAD	(TX % WSIZE)
#define WARP	(TX / WSIZE)

//-------------------------------------------------------------------
// SHARED
//-------------------------------------------------------------------
struct Params {
	uint32_t oWidth;
	uint32_t oHeight;
	uint32_t iWidth;
	uint32_t iHeight;
	float pWidth;
	float pHeight;
	float lambda;
};

//-------------------------------------------------------------------
// DEVICE
//-------------------------------------------------------------------
__device__ __forceinline__ void normalize(float4& var) {
	var.x /= var.w;
	var.y /= var.w;
	var.z /= var.w;
	var.w = 1.0f;
}

//-------------------------------------------------------------------
__device__ __forceinline__ void add(float4& output, const uchar3& color, const float factor) {
	output.x += color.x * factor;	
	output.y += color.y * factor;	
	output.z += color.z * factor;	
	output.w += factor;
}

//-------------------------------------------------------------------
__device__ __forceinline__ void add(float4& output, const float4& color) {
	output.x += color.x;
	output.y += color.y;
	output.z += color.z;
	output.w += color.w;
}

//-------------------------------------------------------------------
__device__ __forceinline__ float lambda(const Params p, const float dist) {
	if(p.lambda == 0.0f)
		return 1.0f;
	else if(p.lambda == 1.0f)
		return dist;

	return pow(dist, p.lambda);
}

//-------------------------------------------------------------------
__device__ __forceinline__ void operator+=(float4& output, const float4 value) {
	output.x += value.x;
	output.y += value.y;
	output.z += value.z;
	output.w += value.w;
}

//-------------------------------------------------------------------
struct Local {
	float sx, ex, sy, ey;
	uint32_t sxr, syr, exr, eyr, xCount, yCount, pixelCount;

	__device__ __forceinline__ Local(const Params& p) {
		sx			= fmaxf(PX		* p.pWidth, 0.0f);
		ex			= fminf((PX+1)	* p.pWidth, p.iWidth);
		sy			= fmaxf(PY		* p.pHeight, 0.0f);
		ey			= fminf((PY+1)	* p.pHeight, p.iHeight);

		sxr			= (uint32_t)floor(sx);
		syr			= (uint32_t)floor(sy);
		exr			= (uint32_t)ceil(ex);
		eyr			= (uint32_t)ceil(ey);
		xCount		= exr - sxr;
		yCount		= eyr - syr;
		pixelCount	= xCount * yCount;
	}
};

//-------------------------------------------------------------------
__device__ __forceinline__ float contribution(const Local& l, float f, const uint32_t x, const uint32_t y) {
	if(x < l.sx)		f *= 1.0f - (l.sx - x);
	if((x+1.0f) > l.ex)	f *= 1.0f - ((x+1.0f) - l.ex);
	if(y < l.sy)		f *= 1.0f - (l.sy - y);
	if((y+1.0f) > l.ey)	f *= 1.0f - ((y+1.0f) - l.ey);
	return f;
}

//-------------------------------------------------------------------
// taken from: https://devblogs.nvidia.com/parallelforall/faster-parallel-reductions-kepler/
__device__ __forceinline__ float4 __shfl_down(const float4 var, const uint32_t srcLane, const uint32_t width = 32) {
	float4 output;
	output.x = __shfl_down(var.x, srcLane, width);
	output.y = __shfl_down(var.y, srcLane, width);
	output.z = __shfl_down(var.z, srcLane, width);
	output.w = __shfl_down(var.w, srcLane, width);
	return output;
}

//-------------------------------------------------------------------
__device__ __forceinline__ void reduce(float4& value) {
	value += __shfl_down(value, 16);
	value += __shfl_down(value, 8);
	value += __shfl_down(value, 4);
	value += __shfl_down(value, 2);
	value += __shfl_down(value, 1);
}

//-------------------------------------------------------------------
__device__ __forceinline__ float distance(const float4& avg, const uchar3& color) {
	const float x = avg.x - color.x;
	const float y = avg.y - color.y;
	const float z = avg.z - color.z;

	return sqrt(x * x + y * y + z * z) / 441.6729559f; // L2-Norm / sqrt(255^2 * 3)
}

//-------------------------------------------------------------------
__global__ void kernelGuidance(const uchar3* __restrict__ input, uchar3* __restrict__ patches, const Params p) {
    if(PX >= p.oWidth || PY >= p.oHeight) return;

	// init
	const Local l(p);
	float4 color = {0};

	// iterate pixels
	for(uint32_t i = WTHREAD; i < l.pixelCount; i += WSIZE) {
		const uint32_t x = l.sxr + (i % l.xCount);
		const uint32_t y = l.syr + (i / l.xCount);
		 
		float f = contribution(l, 1.0f, x, y);	

		const uchar3& pixel = input[x + y * p.iWidth];
		add(color, make_float4(pixel.x * f, pixel.y * f, pixel.z * f, f));
	}

	// reduce warps
	reduce(color);

	// store results
	if((TX % 32) == 0) {
		normalize(color);
		patches[PX + PY * p.oWidth] = make_uchar3(color.x, color.y, color.z);
	}
}

//-------------------------------------------------------------------
__device__ __forceinline__ float4 calcAverage(const Params& p, const uchar3* __restrict__ patches) {
	const float corner	= 1.0;
	const float edge	= 2.0;
	const float center	= 4.0;

	// calculate average color
	float4 avg = {0};

	// TOP
	if(PY > 0) {
		if(PX > 0) 
			add(avg, patches[(PX - 1) + (PY - 1) * p.oWidth], corner);

		add(avg, patches[(PX) + (PY - 1) * p.oWidth], edge);
	
		if((PX+1) < p.oWidth)
			add(avg, patches[(PX + 1) + (PY - 1) * p.oWidth], corner);
	}

	// LEFT
	if(PX > 0) 
		add(avg, patches[(PX - 1) + (PY) * p.oWidth], edge);

	// CENTER
	add(avg, patches[(PX) + (PY) * p.oWidth], center);
	
	// RIGHT
	if((PX+1) < p.oWidth)
		add(avg, patches[(PX + 1) + (PY) * p.oWidth], edge);

	// BOTTOM
	if((PY+1) < p.oHeight) {
		if(PX > 0) 
			add(avg, patches[(PX - 1) + (PY + 1) * p.oWidth], corner);

		add(avg, patches[(PX) + (PY + 1) * p.oWidth], edge);
	
		if((PX+1) < p.oWidth)
			add(avg, patches[(PX + 1) + (PY + 1) * p.oWidth], corner);
	}

	normalize(avg);

	return avg;
}

//-------------------------------------------------------------------
__global__ void kernelDownsampling(const uchar3* __restrict__ input, const uchar3* __restrict__ patches, const Params p, uchar3* __restrict__ output) {
    if(PX >= p.oWidth || PY >= p.oHeight) return;

	// init
	const Local l(p);
	const float4 avg = calcAverage(p, patches);

	float4 color = {0};

	// iterate pixels
	for(uint32_t i = WTHREAD; i < l.pixelCount; i += WSIZE) {
		const uint32_t x = l.sxr + (i % l.xCount);
		const uint32_t y = l.syr + (i / l.xCount);

		const uchar3& pixel = input[x + y * p.iWidth];
		float f = distance(avg, pixel);
		
		f = lambda(p, f);
		f = contribution(l, f, x, y);

		add(color, pixel, f);
	}

	// reduce warp
	reduce(color);

	if(WTHREAD == 0) {
		uchar3& ref = output[PX + PY * p.oWidth];

		if(color.w == 0.0f)
			ref = make_uchar3((unsigned char)avg.x, (unsigned char)avg.y, (unsigned char)avg.z);
		else {
			normalize(color);
			ref = make_uchar3((unsigned char)color.x, (unsigned char)color.y, (unsigned char)color.z);
		}
	}
}

//-------------------------------------------------------------------
// HOST
//-------------------------------------------------------------------
void check(cudaError err) {
	if(err != cudaSuccess) {
		std::cerr << "CUDA_ERROR: " << (int)err << " " << cudaGetErrorName(err) << ": " << cudaGetErrorString(err) << std::endl;
		exit(1);
	}
}

//-------------------------------------------------------------------
void run(const Params& i, const void* hInput, void* hOutput) {
	// calc sizes
	const size_t sInput		= sizeof(uchar3) * i.iWidth * i.iHeight;
	const size_t sOutput	= sizeof(uchar3) * i.oWidth * i.oHeight;
	const size_t sGuidance	= sizeof(uchar3) * i.oWidth * i.oHeight;

	// alloc GPU
	uchar3* dInput = 0, *dOutput = 0, *dGuidance = 0;
	
	check(cudaMalloc(&dInput, sInput));
	check(cudaMalloc(&dOutput, sOutput));
	check(cudaMalloc(&dGuidance, sGuidance));

	// copy data
	check(cudaMemcpy(dInput, hInput, sInput, cudaMemcpyHostToDevice));

	// launch config
	const dim3 threads(THREADS, 1, 1); // 4 warps, 1 warp per patch
	const dim3 blocks((uint32_t)std::ceil(i.oWidth / (double)TSIZE), i.oHeight, 1);
	
	// execute kernels
	kernelGuidance		<<<blocks, threads>>>(dInput, dGuidance, i);
	kernelDownsampling	<<<blocks, threads>>>(dInput, dGuidance, i, dOutput);

	// copy data
	check(cudaMemcpy(hOutput, dOutput, sOutput, cudaMemcpyDeviceToHost));

	// free GPU
	check(cudaFree(dInput));
	check(cudaFree(dOutput));
	check(cudaFree(dGuidance));
	
	// reset device
	//check(cudaDeviceReset());
}