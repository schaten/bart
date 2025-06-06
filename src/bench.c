/* Copyright 2014. The Regents of the University of California.
 * Copyright 2015-2018. Martin Uecker.
 * All rights reserved. Use of this source code is governed by
 * a BSD-style license which can be found in the LICENSE file.
 * 
 * Authors: 
 * 2014-2018 Martin Uecker
 * 2014 Jonathan Tamir
 */

#include <stdio.h>
#include <assert.h>
#include <complex.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>

#include "num/multind.h"
#include "num/flpmath.h"
#include "num/rand.h"
#include "num/init.h"
#include "num/ops_p.h"
#include "num/mdfft.h"
#include "num/fft.h"
#include "num/ode.h"
#include "num/filter.h"

#include "wavelet/wavthresh.h"

#include "misc/debug.h"
#include "misc/misc.h"
#include "misc/mmio.h"
#include "misc/opts.h"

#define DIMS 8




static double bench_generic_copy(long dims[DIMS])
{
	long strs[DIMS];

	md_calc_strides(DIMS, strs, dims, CFL_SIZE);
	md_calc_strides(DIMS, strs, dims, CFL_SIZE);

	complex float* x = md_alloc(DIMS, dims, CFL_SIZE);
	complex float* y = md_alloc(DIMS, dims, CFL_SIZE);

	md_gaussian_rand(DIMS, dims, x);

	double tic = timestamp();

	md_copy2(DIMS, dims, strs, y, strs, x, CFL_SIZE);

	double toc = timestamp();

	md_free(x);
	md_free(y);

	return toc - tic;
}

	
static double bench_generic_matrix_multiply(long dims[DIMS])
{
	long dimsX[DIMS];
	long dimsY[DIMS];
	long dimsZ[DIMS];
#if 1
	md_select_dims(DIMS, 2 * 3 + 17, dimsX, dims);	// 1 110 1
	md_select_dims(DIMS, 2 * 6 + 17, dimsY, dims);	// 1 011 1
	md_select_dims(DIMS, 2 * 5 + 17, dimsZ, dims);	// 1 101 1
#else
	md_select_dims(DIMS, 2 * 5 + 17, dimsZ, dims);	// 1 101 1
	md_select_dims(DIMS, 2 * 3 + 17, dimsY, dims);	// 1 110 1
	md_select_dims(DIMS, 2 * 6 + 17, dimsX, dims);	// 1 011 1
#endif
	complex float* x = md_alloc(DIMS, dimsX, CFL_SIZE);
	complex float* y = md_alloc(DIMS, dimsY, CFL_SIZE);
	complex float* z = md_alloc(DIMS, dimsZ, CFL_SIZE);

	md_gaussian_rand(DIMS, dimsX, x);
	md_gaussian_rand(DIMS, dimsY, y);

	double tic = timestamp();

	md_ztenmul(DIMS, dimsZ, z, dimsX, x, dimsY, y);

	double toc = timestamp();


	md_free(x);
	md_free(y);
	md_free(z);

	return toc - tic;
}


static double bench_generic_add(long dims[DIMS], unsigned long flags, bool forloop)
{
	long dimsX[DIMS];
	long dimsY[DIMS];

	long dimsC[DIMS];

	md_select_dims(DIMS, flags, dimsX, dims);
	md_select_dims(DIMS, ~flags, dimsC, dims);
	md_select_dims(DIMS, ~0UL, dimsY, dims);

	long strsX[DIMS];
	long strsY[DIMS];

	md_calc_strides(DIMS, strsX, dimsX, CFL_SIZE);
	md_calc_strides(DIMS, strsY, dimsY, CFL_SIZE);

	complex float* x = md_alloc(DIMS, dimsX, CFL_SIZE);
	complex float* y = md_alloc(DIMS, dimsY, CFL_SIZE);

	md_gaussian_rand(DIMS, dimsX, x);
	md_gaussian_rand(DIMS, dimsY, y);

	long L = md_calc_size(DIMS, dimsC);
	long T = md_calc_size(DIMS, dimsX);

	double tic = timestamp();

	if (forloop) {

		for (long i = 0; i < L; i++) {

			for (long j = 0; j < T; j++)
				y[i + j * L] += x[j];
		}

	} else {

		md_zaxpy2(DIMS, dims, strsY, y, 1., strsX, x);
	}

	double toc = timestamp();


	md_free(x);
	md_free(y);

	return toc - tic;
}


static double bench_generic_sum(long dims[DIMS], unsigned long flags, bool forloop)
{
	long dimsX[DIMS];
	long dimsY[DIMS];
	long dimsC[DIMS];

	md_select_dims(DIMS, ~0UL, dimsX, dims);
	md_select_dims(DIMS, flags, dimsY, dims);
	md_select_dims(DIMS, ~flags, dimsC, dims);

	long strsX[DIMS];
	long strsY[DIMS];

	md_calc_strides(DIMS, strsX, dimsX, CFL_SIZE);
	md_calc_strides(DIMS, strsY, dimsY, CFL_SIZE);

	complex float* x = md_alloc(DIMS, dimsX, CFL_SIZE);
	complex float* y = md_alloc(DIMS, dimsY, CFL_SIZE);

	md_gaussian_rand(DIMS, dimsX, x);
	md_clear(DIMS, dimsY, y, CFL_SIZE);

	long L = md_calc_size(DIMS, dimsC);
	long T = md_calc_size(DIMS, dimsY);

	double tic = timestamp();

	if (forloop) {

		for (long i = 0; i < L; i++) {

			for (long j = 0; j < T; j++)
				y[j] = y[j] + x[i + j * L];
		}

	} else {

		md_zaxpy2(DIMS, dims, strsY, y, 1., strsX, x);
	}

	double toc = timestamp();


	md_free(x);
	md_free(y);

	return toc - tic;
}

static double bench_copy1(long scale)
{
	long dims[DIMS] = { 1, 128 * scale, 128 * scale, 1, 1, 16, 1, 16 };
	return bench_generic_copy(dims);
}

static double bench_copy2(long scale)
{
	long dims[DIMS] = { 262144 * scale, 16, 1, 1, 1, 1, 1, 1 };
	return bench_generic_copy(dims);
}


static double bench_matrix_mult(long scale)
{
	long dims[DIMS] = { 1, 256 * scale, 256 * scale, 256 * scale, 1, 1, 1, 1 };
	return bench_generic_matrix_multiply(dims);
}



static double bench_batch_matmul1(long scale)
{
	long dims[DIMS] = { 30000 * scale, 8, 8, 8, 1, 1, 1, 1 };
	return bench_generic_matrix_multiply(dims);
}



static double bench_batch_matmul2(long scale)
{
	long dims[DIMS] = { 1, 8, 8, 8, 30000 * scale, 1, 1, 1 };
	return bench_generic_matrix_multiply(dims);
}


static double bench_tall_matmul1(long scale)
{
	long dims[DIMS] = { 1, 8, 8, 100000 * scale, 1, 1, 1, 1 };
	return bench_generic_matrix_multiply(dims);
}


static double bench_tall_matmul2(long scale)
{
	long dims[DIMS] = { 1, 100000 * scale, 8, 8, 1, 1, 1, 1 };
	return bench_generic_matrix_multiply(dims);
}


static double bench_add(long scale)
{
	long dims[DIMS] = { 65536 * scale, 1, 50 * scale, 1, 1, 1, 1, 1 };
	return bench_generic_add(dims, MD_BIT(2), false);
}

static double bench_addf(long scale)
{
	long dims[DIMS] = { 65536 * scale, 1, 50 * scale, 1, 1, 1, 1, 1 };
	return bench_generic_add(dims, MD_BIT(2), true);
}

static double bench_add2(long scale)
{
	long dims[DIMS] = { 50 * scale, 1, 65536 * scale, 1, 1, 1, 1, 1 };
	return bench_generic_add(dims, MD_BIT(0), false);
}

static double bench_sum2(long scale)
{
	long dims[DIMS] = { 50 * scale, 1, 65536 * scale, 1, 1, 1, 1, 1 };
	return bench_generic_sum(dims, MD_BIT(0), false);
}

static double bench_sum(long scale)
{
	long dims[DIMS] = { 65536 * scale, 1, 50 * scale, 1, 1, 1, 1, 1 };
	return bench_generic_sum(dims, MD_BIT(2), false);
}

static double bench_sumf(long scale)
{
	long dims[DIMS] = { 65536 * scale, 1, 50 * scale, 1, 1, 1, 1, 1 };
	return bench_generic_sum(dims, MD_BIT(2), true);
}


static double bench_zmul(long scale)
{
	long dimsx[DIMS] = { 256, 256, 1, 1, 90 * scale, 1, 1, 1 };
	long dimsy[DIMS] = { 256, 256, 1, 1,  1, 1, 1, 1 };
	long dimsz[DIMS] = {   1,   1, 1, 1, 90 * scale, 1, 1, 1 };

	complex float* x = md_alloc(DIMS, dimsx, CFL_SIZE);
	complex float* y = md_alloc(DIMS, dimsy, CFL_SIZE);
	complex float* z = md_alloc(DIMS, dimsz, CFL_SIZE);

	md_gaussian_rand(DIMS, dimsy, y);
	md_gaussian_rand(DIMS, dimsz, z);

	long strsx[DIMS];
	long strsy[DIMS];
	long strsz[DIMS];

	md_calc_strides(DIMS, strsx, dimsx, CFL_SIZE);
	md_calc_strides(DIMS, strsy, dimsy, CFL_SIZE);
	md_calc_strides(DIMS, strsz, dimsz, CFL_SIZE);

	double tic = timestamp();

	md_zmul2(DIMS, dimsx, strsx, x, strsy, y, strsz, z);

	double toc = timestamp();

	md_free(x);
	md_free(y);
	md_free(z);

	return toc - tic;
}


static double bench_transpose(long scale)
{
	long dims[DIMS] = { 2000 * scale, 2000 * scale, 1, 1, 1, 1, 1, 1 };

	complex float* x = md_alloc(DIMS, dims, CFL_SIZE);
	complex float* y = md_alloc(DIMS, dims, CFL_SIZE);
	
	md_gaussian_rand(DIMS, dims, x);
	md_clear(DIMS, dims, y, CFL_SIZE);

	double tic = timestamp();

	md_transpose(DIMS, 0, 1, dims, y, dims, x, CFL_SIZE);

	double toc = timestamp();

	md_free(x);
	md_free(y);
	
	return toc - tic;
}



static double bench_resize(long scale)
{
	long dimsX[DIMS] = { 2000 * scale, 1000 * scale, 1, 1, 1, 1, 1, 1 };
	long dimsY[DIMS] = { 1000 * scale, 2000 * scale, 1, 1, 1, 1, 1, 1 };

	complex float* x = md_alloc(DIMS, dimsX, CFL_SIZE);
	complex float* y = md_alloc(DIMS, dimsY, CFL_SIZE);
	
	md_gaussian_rand(DIMS, dimsX, x);
	md_clear(DIMS, dimsY, y, CFL_SIZE);

	double tic = timestamp();

	md_resize(DIMS, dimsY, y, dimsX, x, CFL_SIZE);

	double toc = timestamp();

	md_free(x);
	md_free(y);
	
	return toc - tic;
}


static double bench_norm(int s, long scale)
{
	long dims[DIMS] = { 256 * scale, 256 * scale, 1, 16, 1, 1, 1, 1 };
#if 0
	complex float* x = md_alloc_gpu(DIMS, dims, CFL_SIZE);
	complex float* y = md_alloc_gpu(DIMS, dims, CFL_SIZE);
#else
	complex float* x = md_alloc(DIMS, dims, CFL_SIZE);
	complex float* y = md_alloc(DIMS, dims, CFL_SIZE);
#endif
	
	md_gaussian_rand(DIMS, dims, x);
	md_gaussian_rand(DIMS, dims, y);

	double tic = timestamp();

	switch (s) {
	case 0:
		md_zscalar(DIMS, dims, x, y);
		break;
	case 1:
		md_zscalar_real(DIMS, dims, x, y);
		break;
	case 2:
		md_znorm(DIMS, dims, x);
		break;
	case 3:
		md_z1norm(DIMS, dims, x);
		break;
	}

	double toc = timestamp();

	md_free(x);
	md_free(y);
	
	return toc - tic;
}

static double bench_zscalar(long scale)
{
	return bench_norm(0, scale);
}

static double bench_zscalar_real(long scale)
{
	return bench_norm(1, scale);
}

static double bench_znorm(long scale)
{
	return bench_norm(2, scale);
}

static double bench_zl1norm(long scale)
{
	return bench_norm(3, scale);
}


static double bench_wavelet(long scale)
{
	long dims[DIMS] = { 1, 256 * scale, 256 * scale, 1, 16, 1, 1, 1 };
	long minsize[DIMS] = { [0 ... DIMS - 1] = 1 };
	minsize[0] = MIN(dims[0], 16);
	minsize[1] = MIN(dims[1], 16);
	minsize[2] = MIN(dims[2], 16);

	const struct operator_p_s* p = prox_wavelet_thresh_create(DIMS, dims, 6, 0u, WAVELET_DAU2, minsize, 1.1, true);

	complex float* x = md_alloc(DIMS, dims, CFL_SIZE);
	md_gaussian_rand(DIMS, dims, x);

	double tic = timestamp();

	operator_p_apply(p, 0.98, DIMS, dims, x, DIMS, dims, x);

	double toc = timestamp();

	md_free(x);
	operator_p_free(p);

	return toc - tic;
}


static double bench_generic_mdfft(long dims[DIMS], unsigned long flags)
{
	complex float* x = md_alloc(DIMS, dims, CFL_SIZE);
	complex float* y = md_alloc(DIMS, dims, CFL_SIZE);

	md_gaussian_rand(DIMS, dims, x);

	double tic = timestamp();

	md_fft(DIMS, dims, flags, 0u, y, x);

	double toc = timestamp();

	md_free(x);
	md_free(y);

	return toc - tic;
}

static double bench_mdfft(long scale)
{
	long dims[DIMS] = { 1, 128 * scale, 128 * scale, 1, 1, 4, 1, 4 };
	return bench_generic_mdfft(dims, 6ul);
}



static double bench_generic_fft(long dims[DIMS], unsigned long flags)
{
	complex float* x = md_alloc(DIMS, dims, CFL_SIZE);
	complex float* y = md_alloc(DIMS, dims, CFL_SIZE);

	md_gaussian_rand(DIMS, dims, x);

	double tic = timestamp();

	fft(DIMS, dims, flags, y, x);

	double toc = timestamp();

	md_free(x);
	md_free(y);

	return toc - tic;
}



static double bench_fft(long scale)
{
	long dims[DIMS] = { 1, 256 * scale, 256 * scale, 1, 1, 16, 1, 8 };
	return bench_generic_fft(dims, 6ul);
}




static double bench_generic_fftmod(long dims[DIMS], unsigned long flags)
{
	complex float* x = md_alloc(DIMS, dims, CFL_SIZE);
	complex float* y = md_alloc(DIMS, dims, CFL_SIZE);

	md_gaussian_rand(DIMS, dims, x);

	double tic = timestamp();

	fftmod(DIMS, dims, flags, y, x);

	double toc = timestamp();

	md_free(x);
	md_free(y);

	return toc - tic;
}



static double bench_fftmod(long scale)
{
	long dims[DIMS] = { 1, 256 * scale, 256 * scale, 1, 1, 16, 1, 16 };
	return bench_generic_fftmod(dims, 6ul);
}


enum bench_typ { BENCH_ZFILL, BENCH_ZSMUL, BENCH_LINPHASE };

static double bench_generic_expand(enum bench_typ typ, long scale)
{
	long dims[DIMS] = { 1, 256 * scale, 256 * scale, 1, 1, 16, 1, 16 };

	float linphase_pos[DIMS] = { 0.5, 0.1 };

	complex float* x = md_alloc(DIMS, dims, CFL_SIZE);

	double tic = timestamp();

	switch (typ) {

	case BENCH_ZFILL:
		md_zfill(DIMS, dims, x, 1.);
		break;

	case BENCH_ZSMUL:
		md_zsmul(DIMS, dims, x, x, 1.);
		break;

	case BENCH_LINPHASE:

		linear_phase(DIMS, dims, linphase_pos, x);
		break;

	default:
		assert(0);
	}

	double toc = timestamp();

	md_free(x);

	return toc - tic;
}


static double bench_zfill(long scale)
{
	return bench_generic_expand(BENCH_ZFILL, scale);
}

static double bench_zsmul(long scale)
{
	return bench_generic_expand(BENCH_ZSMUL, scale);
}

static double bench_linphase(long scale)
{
	return bench_generic_expand(BENCH_LINPHASE, scale);
}



static double bench_ode(long scale)
{
	float mat[2][2] = { { 0., +1. }, { -1., 0. } };

	float x[2] = { 1., 0. };
	float h = 10.;
	float tol = 1.E-6;

	double tic = timestamp();

	ode_matrix_interval(h, tol, 2, x, 0., scale * 10001. * M_PI, mat);

	double err = pow(fabs(x[0] + 1.), 2.) + pow(fabs(x[1] - 0.), 2.);
	assert(err < 1.E-2);

	double toc = timestamp();

	return toc - tic;
}



enum bench_indices { REPETITION_IND, SCALE_IND, THREADS_IND, TESTS_IND, BENCH_DIMS };

typedef double (*bench_fun)(long scale);

static void do_test(const long dims[BENCH_DIMS], complex float* out, long scale, bench_fun fun, const char* str)
{
	printf("%30.30s |", str);
	
	int N = (int)dims[REPETITION_IND];
	double sum = 0.;
	double min = 1.E10;
	double max = 0.;

	for (int i = 0; i < N; i++) {

		double dt = fun(scale);
		sum += dt;
		min = MIN(dt, min);
		max = MAX(dt, max);

		printf(" %3.4f", (float)dt);
		fflush(stdout);

		assert(0 == REPETITION_IND);
		out[i] = dt;
	}

	printf(" | Avg: %3.4f Max: %3.4f Min: %3.4f\n", (float)(sum / N), max, min); 
}


const struct benchmark_s {

	bench_fun fun;
	const char* str;

} benchmarks[] = {
	{ bench_add,		"add (md_zaxpy)" },
	{ bench_add2,		"add (md_zaxpy), contiguous" },
	{ bench_addf,		"add (for loop)" },
	{ bench_sum,   		"sum (md_zaxpy)" },
	{ bench_sum2,   	"sum (md_zaxpy), contiguous" },
	{ bench_sumf,   	"sum (for loop)" },
	{ bench_zmul,   	"complex mult. (md_zmul2)" },
	{ bench_transpose,	"complex transpose" },
	{ bench_resize,   	"complex resize" },
	{ bench_matrix_mult,	"complex matrix multiply" },
	{ bench_batch_matmul1,	"batch matrix multiply 1" },
	{ bench_batch_matmul2,	"batch matrix multiply 2" },
	{ bench_tall_matmul1,	"tall matrix multiply 1" },
	{ bench_tall_matmul2,	"tall matrix multiply 2" },
	{ bench_zscalar,	"complex dot product" },
	{ bench_zscalar,	"complex dot product" },
	{ bench_zscalar_real,	"real complex dot product" },
	{ bench_znorm,		"l2 norm" },
	{ bench_zl1norm,	"l1 norm" },
	{ bench_copy1,		"copy 1" },
	{ bench_copy2,		"copy 2" },
	{ bench_zfill,		"complex fill" },
	{ bench_zsmul,		"complex scalar multiplication" },
	{ bench_linphase,	"linear phase" },
	{ bench_wavelet,	"wavelet soft thresh" },
	{ bench_mdfft,		"(MD-)FFT" },
	{ bench_fft,		"FFT" },
	{ bench_fftmod,		"fftmod" },
	{ bench_ode,		"ODE" },
};


static const char help_str[] = "Performs a series of micro-benchmarks.";



int main_bench(int argc, char* argv[argc])
{
	const char* out_file = NULL;

	struct arg_s args[] = {

		ARG_OUTFILE(false, &out_file, "output"),
	};

	bool threads = false;
	bool scaling = false;
	unsigned long flags = ~0UL;

	const struct opt_s opts[] = {

		OPT_SET('T', &threads, "varying number of threads"),
		OPT_SET('S', &scaling, "varying problem size"),
		OPT_ULONG('s', &flags, "flags", "select benchmarks"),
	};

	cmdline(&argc, argv, ARRAY_SIZE(args), args, help_str, ARRAY_SIZE(opts), opts);

	long dims[BENCH_DIMS] = { [0 ... BENCH_DIMS - 1] = 1 };
	long strs[BENCH_DIMS];
	long pos[BENCH_DIMS] = { };

	dims[REPETITION_IND] = 5;
	dims[THREADS_IND] = threads ? 8 : 1;
	dims[SCALE_IND] = scaling ? 5 : 1;
	dims[TESTS_IND] = sizeof(benchmarks) / sizeof(benchmarks[0]);

	md_calc_strides(BENCH_DIMS, strs, dims, CFL_SIZE);

	bool outp = (NULL != out_file);
	complex float* out = (outp ? create_cfl : anon_cfl)(out_file, BENCH_DIMS, dims);

	num_init();

	md_clear(BENCH_DIMS, dims, out, CFL_SIZE);

	do {
		if (!(flags & MD_BIT(pos[TESTS_IND])))
			continue;

		if (threads) {

			num_set_num_threads((int)pos[THREADS_IND] + 1);
			debug_printf(DP_INFO, "%02ld threads. ", pos[THREADS_IND] + 1);
		}

		do_test(dims, &MD_ACCESS(BENCH_DIMS, strs, pos, out), pos[SCALE_IND] + 1,
			benchmarks[pos[TESTS_IND]].fun, benchmarks[pos[TESTS_IND]].str);

	} while (md_next(BENCH_DIMS, dims, ~MD_BIT(REPETITION_IND), pos));

	unmap_cfl(BENCH_DIMS, dims, out);

	return 0;
}


