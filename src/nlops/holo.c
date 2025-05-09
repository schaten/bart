#include <complex.h>

#include "linops/someops.h"
#include "misc/types.h"
#include "misc/misc.h"
#include "misc/mri.h"

#include "num/multind.h"
#include "num/flpmath.h"

#include "nlops/nlop.h"
#include "nlops/chain.h"
#include "nlops/nlop_jacobian.h"
#include "nlops/cast.h"

#include "nlops/zexp.h"
#include "nlops/someops.h"

#include "linops/fmac.h"

#include "holo.h"

struct holo_s {

	INTERFACE(nlop_data_t);
};

DEF_TYPEID(holo_s);

static void holo_free(const nlop_data_t* _data)
{
	xfree(_data);
}

static void holo_apply(const nlop_data_t* /*_data*/, int N, const long dims[N], complex float* dst, const complex float* src, complex float* der)
{
	//TODO

	if (NULL != der)
		assert(false);
}

struct nlop_s* nlop_holo_create(int N, const long dims[N], const complex float* kernel)
{
	PTR_ALLOC(struct holo_s, data);
	SET_TYPEID(holo_s, data);

	const struct nlop_s* exp1 = nlop_zexp_create(N, dims);
	const struct nlop_s* conv = nlop_from_linop_F(linop_conv_create(N, READ_FLAG | PHS1_FLAG, CONV_CYCLIC, CONV_SYMMETRIC, dims, dims, dims, kernel));

	const struct nlop_s* zabs = nlop_zabs_create(N, dims);

	const struct nlop_s* sqr = nlop_zspow_create(N, dims, 2.);

	return nlop_chain_FF(nlop_chain_FF(nlop_chain_FF(exp1, conv), zabs), sqr);
}
