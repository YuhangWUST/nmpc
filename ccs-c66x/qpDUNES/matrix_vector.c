/*
 *  This file is part of qpDUNES.
 *
 *  qpDUNES -- A DUal NEwton Strategy for convex quadratic programming.
 *  Copyright (C) 2012 by Janick Frasch, Hans Joachim Ferreau et al.
 *  All rights reserved.
 *
 *  qpDUNES is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  qpDUNES is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with qpDUNES; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#include <stddef.h>
#include <stdint.h>
#include <assert.h>

#include "matrix_vector.h"
#include "../c66math.h"

/* Matrix-vector product y = z'*H*z */
real_t multiplyzHz(const vv_matrix_t* const H, const z_vector_t* const z,
const size_t nV) {
    return multiplyVectorMatrixVector((matrix_t*)H, (vector_t*)z, nV);
}


/* Matrix-vector product y = invH*z */
return_t multiplyInvHz(qpData_t* const qpData, z_vector_t* const res,
const vv_matrix_t* const cholH, const z_vector_t* const z, const size_t nV) {
    return multiplyInvMatrixVector(qpData, (vector_t*)res, (matrix_t*)cholH,
                                   (vector_t*)z, nV);
}


/* Matrix-vector product res = C*z */
return_t multiplyCz(qpData_t* const qpData, x_vector_t* const res,
const xz_matrix_t* const C, const z_vector_t* const z) {
    assert(qpData && res && C && z && res->data && C->data && z->data);
    _nassert((size_t)res->data % 4 == 0);
    _nassert((size_t)C->data % 4 == 0);
    _nassert((size_t)z->data % 4 == 0);

    /* only dense multiplication */
    size_t i, j;

    #pragma MUST_ITERATE(_NX_, _NX_)
    for (i = 0; i < _NX_; i++) {
        res->data[i] = 0.0;
        #pragma MUST_ITERATE(_NZ_, _NZ_)
        for (j = 0; j < _NZ_; j++) {
            res->data[i] += accC(i, j) * z->data[j];
        }
    }

    return QPDUNES_OK;
}


/* Matrix-vector product z = C.T*y */
return_t multiplyCTy(qpData_t* const qpData, z_vector_t* const res,
const xz_matrix_t* const C, const x_vector_t* const y) {
    assert(qpData && res && C && y && res->data && C->data && y->data);
    _nassert((size_t)res->data % 4 == 0);
    _nassert((size_t)C->data % 4 == 0);
    _nassert((size_t)y->data % 4 == 0);

    /* only dense multiplication */
    size_t i, j;

    /* change multiplication order for more efficient memory access */
    #pragma MUST_ITERATE(_NZ_, _NZ_)
    for (j = 0; j < _NZ_; j++) {
        res->data[j] = 0.0;
    }

    #pragma MUST_ITERATE(_NX_, _NX_)
    for (i = 0; i < _NX_; i++) {
        #pragma MUST_ITERATE(_NZ_, _NZ_)
        for (j = 0; j < _NZ_; j++) {
            res->data[j] += accC(i, j) * y->data[i];
        }
    }

    return QPDUNES_OK;
}


/* Matrix times inverse matrix product res = A * Q^-1 */
return_t multiplyAInvQ(qpData_t* const qpData,
xx_matrix_t* restrict const res, const xx_matrix_t* const C,
const vv_matrix_t* const cholH) {
    assert(qpData && res && C && cholH && res->data && C->data &&
           cholH->data);
    _nassert((size_t)res->data % 4 == 0);
    _nassert((size_t)C->data % 4 == 0);
    _nassert((size_t)cholH->data % 4 == 0);

    size_t i, j;

    res->sparsityType = QPDUNES_DENSE;

    /* choose appropriate multiplication routine */
    switch (cholH->sparsityType) {
        /* cholH invalid types */
        case QPDUNES_DENSE:
        case QPDUNES_SPARSE:
        case QPDUNES_MATRIX_UNDEFINED:
        case QPDUNES_ALLZEROS:
            assert(0 && "Invalid cholH sparsity type");
            return QPDUNES_ERR_UNKNOWN_MATRIX_SPARSITY_TYPE;
        /* cholH diagonal */
        case QPDUNES_DIAGONAL:
            /* scale A part of C column-wise */
            #pragma MUST_ITERATE(_NX_, _NX_)
            for (i = 0; i < _NX_; i++) {
                #pragma MUST_ITERATE(_NX_, _NX_)
                for (j = 0; j < _NX_; j++) {
                    /*
                    cholH is the actual matrix in diagonal case -- elements
                    are stored as reciprocal
                    */
                    res->data[i * _NX_ + j] =
                        accC(i, j) * cholH->data[j];
                }
            }
            break;
        /* cholH identity */
        case QPDUNES_IDENTITY:
            /* copy A block */
            #pragma MUST_ITERATE(_NX_, _NX_)
            for (i = 0; i < _NX_; i++) {
                #pragma MUST_ITERATE(_NX_, _NX_)
                for (j = 0; j < _NX_; j++) {
                    res->data[i * _NX_ + j] = accC(i, j);
                }
            }
            break;
    }

    return QPDUNES_OK;
}


/* Inverse matrix times identity matrix product res = Q^-1 * I */
return_t getInvQ(qpData_t* const qpData, xx_matrix_t* const res,
const vv_matrix_t* const cholH, size_t nV) {
    assert(qpData && res && cholH && res->data && cholH->data);
    _nassert((size_t)res->data % 4 == 0);
    _nassert((size_t)cholH->data % 4 == 0);

    switch (cholH->sparsityType){
        /* cholM1 dense */
        case QPDUNES_DENSE:
        case QPDUNES_SPARSE:
        case QPDUNES_MATRIX_UNDEFINED:
        case QPDUNES_ALLZEROS:
            assert(0 && "Invalid cholH sparsity type");
            return QPDUNES_ERR_UNKNOWN_MATRIX_SPARSITY_TYPE;
        /* cholM1 diagonal */
        case QPDUNES_DIAGONAL:
            res->sparsityType = QPDUNES_DIAGONAL;
            /* cholH is just save in first line; first _NX_ elements are Q part */
            return backsolveMatrixDiagonalIdentity(qpData, res->data,
                                                   cholH->data, nV);
        /* cholM1 identity */
        case QPDUNES_IDENTITY:
            res->sparsityType = QPDUNES_IDENTITY;
            return QPDUNES_OK;
    }
}

return_t factorizeH(qpData_t* const qpData, vv_matrix_t* const cholH,
const vv_matrix_t* const H, size_t nV) {
    return factorizePosDefMatrix(qpData, (matrix_t*)cholH, (matrix_t*)H, nV);
}

/* M2 * M1^-1 * M2.T -- result gets added to res, not overwritten */
return_t addCInvHCT(qpData_t* const qpData, xx_matrix_t* const restrict res,
const vv_matrix_t* const restrict cholM1, const xz_matrix_t* const restrict M2,
const d2_vector_t* const y, /* vector containing non-zeros for columns of M2 to be eliminated */
zx_matrix_t* const restrict zxMatTmp) { /* temporary matrix of shape dim1 x dim0 */
    real_t* restrict yd = y->data;

    size_t i, j, l;

    assert(cholM1->sparsityType == QPDUNES_DIAGONAL);

    /* compute M1^-1/2 * M2.T */

    /* computes full inverse times M2! */
    backsolveMatrixTDiagonalDense(qpData, zxMatTmp->data,
                                  cholM1->data, M2->data, _NX_, _NZ_);

    qpDUNES_makeMatrixDense(res, _NX_, _NX_);

    /*
    Z already contains H^-1 * M2^T, therefore only multiplication with M2
    from left is needed

    compute M2 * Z as dyadic products
    */
    for (l = 0; l < _NZ_; l++) {
        /*
        only add columns of variables with inactive upper and lower bounds
        */
        if ((yd[2u * l] <= qpData->options.equalityTolerance) &&
                (yd[2u * l + 1u] <= qpData->options.equalityTolerance)) {
            #pragma MUST_ITERATE(_NX_, _NX_)
            for (i = 0; i < _NX_; i++) {
                #pragma MUST_ITERATE(_NX_, _NX_)
                for (j = 0; j < _NX_; j++) {
                    /* since M2 is dense, so is Z */
                    res->data[i * _NX_ + j] +=
                        M2->data[i * _NZ_ + l] *
                        zxMatTmp->data[l * _NX_ + j];
                }
            }
        } /* end of dyadic addend */
    }

    return QPDUNES_OK;
}


return_t addScaledVector(vector_t* restrict const res, real_t scalingFactor,
const vector_t* restrict const update, size_t len) {
    assert(res && update && res->data && update->data);
    _nassert((size_t)res->data % 4 == 0);
    _nassert((size_t)update->data % 4 == 0);

    size_t i;

    for (i = 0; i < len; i++) {
        res->data[i] += scalingFactor * update->data[i];
    }

    return QPDUNES_OK;
}


/* res = x + a*y  */
return_t addVectorScaledVector(vector_t* restrict const res,
const vector_t* restrict const x, real_t scalingFactor,
const vector_t* restrict const y, size_t len) {
    assert(res && x && y && res->data && x->data && y->data);
    _nassert((size_t)res->data % 4 == 0);
    _nassert((size_t)x->data % 4 == 0);
    _nassert((size_t)y->data % 4 == 0);

    size_t i;

    for (i = 0; i < len; i++) {
        res->data[i] = x->data[i] + scalingFactor * y->data[i];
    }

    return QPDUNES_OK;
}


return_t addToVector(vector_t* restrict const res,
const vector_t* restrict const update, size_t len) {
    assert(res && update && res->data && update->data);
    _nassert((size_t)res->data % 4 == 0);
    _nassert((size_t)update->data % 4 == 0);

    size_t i;

    for (i = 0; i < len; i++) {
        res->data[i] += update->data[i];
    }

    return QPDUNES_OK;
}


return_t subtractFromVector(vector_t* restrict const res,
const vector_t* const update, size_t len) {
    assert(res && update && res->data && update->data);
    _nassert((size_t)res->data % 4 == 0);
    _nassert((size_t)update->data % 4 == 0);

    size_t i;

    for (i = 0; i < len; i++) {
        res->data[i] -= update->data[i];
    }

    return QPDUNES_OK;
}


return_t negateVector(vector_t* const res, size_t len) {
    assert(res && res->data);
    _nassert((size_t)res->data % 4 == 0);

    size_t i;

    for (i = 0; i < len; i++) {
        res->data[i] = -res->data[i];
    }

    return QPDUNES_OK;
}


/* Compute a Cholesky factorization of M */
return_t factorizePosDefMatrix(qpData_t* const qpData, matrix_t* const cholM,
const matrix_t* const M, size_t dim0) {
    size_t i;
    /* choose appropriate factorization routine */
    switch (M->sparsityType) {
        case QPDUNES_DIAGONAL:
            /*
            cholM in this case is defined to contain full diagonal matrix
            (not a factor)

            matrix diagonal is saved in first line -- take the reciprocal of
            each element because the cholH elements are always used to divide
            elsewhere.
            */
            for (i = 0; i < dim0; i++) {
                cholM->data[i] = recip_f(M->data[i]);
            }
            cholM->sparsityType = QPDUNES_DIAGONAL;
            return QPDUNES_OK;
        case QPDUNES_IDENTITY:
            cholM->sparsityType = QPDUNES_IDENTITY;
            return QPDUNES_OK;
        case QPDUNES_DENSE:
        case QPDUNES_SPARSE:
        case QPDUNES_MATRIX_UNDEFINED:
        case QPDUNES_ALLZEROS:
            assert(0 && "Invalid M sparsity type");
            return QPDUNES_ERR_UNKNOWN_MATRIX_SPARSITY_TYPE;
    }
}


/* Backsolve for a diagonal M compute res for M*res = b */
return_t backsolveDiagonal(qpData_t* const qpData, real_t* const res,
const real_t* const M, const real_t* const b, size_t n) {
    assert(qpData && res && M && b);
    assert(n);
    _nassert((size_t)res % 4 == 0);
    _nassert((size_t)M % 4 == 0);
    _nassert((size_t)b % 4 == 0);

    size_t i;

    /* Solve M*res = b -- M elements are always reciprocal */
    for (i = 0; i < n; i++) {
        res[i] = b[i] * accM(0, i, n);
    }

    return QPDUNES_OK;
}


/* Matrix backsolve for M1 diagonal, M2 identity compute res for M1*res = I */
return_t backsolveMatrixDiagonalIdentity(qpData_t* const qpData,
real_t* const res, const real_t* const M1, size_t dim0) {
    assert(qpData && res && M1);
    _nassert((size_t)res % 4 == 0);
    _nassert((size_t)M1 % 4 == 0);

    size_t i;

    /* backsolve on diagonal matrix: res for M1*res = I */
    for (i = 0; i < dim0; i++) {
        if (abs_f(M1[i]) >= qpData->options.QPDUNES_ZERO) {
            /*
            M1 is the actual matrix in diagonal case -- all elements are
            reciprocal
            */
            res[i] = M1[i];
        } else {
            return QPDUNES_ERR_DIVISION_BY_ZERO;
        }
    }

    return QPDUNES_OK;
}


/* Matrix backsolve for L diagonal and M dense compute res for L*res = M^T */
return_t backsolveMatrixTDiagonalDense(qpData_t* const qpData,
real_t* const res, const real_t* const M1,
const real_t* const M2, /* untransposed M2 */
size_t dim0, size_t dim1) { /* dimensions of M2 */
    assert(qpData && res && M1 && M2);
    _nassert((size_t)res % 4 == 0);
    _nassert((size_t)M1 % 4 == 0);
    _nassert((size_t)M2 % 4 == 0);

    size_t i, j;

    for (i = 0; i < dim1; i++) {
        for (j = 0; j < dim0; j++) {
            if (abs_f(M1[i]) >=
                    qpData->options.QPDUNES_ZERO * abs_f(M2[j * dim1 + i])) {
                /*
                M1 is the actual matrix in diagonal case; M2 is untransposed
                All elements of M1 are reciprocal
                */
                res[i * dim0 + j] = M2[j * dim1 + i] * M1[i];
            } else {
                return QPDUNES_ERR_DIVISION_BY_ZERO;
            }
        }
    }

    return QPDUNES_OK;
}


/*
Generic vector-matrix-vector product b = x'*M*x - M has to be square matrix
*/
real_t multiplyVectorMatrixVector(const matrix_t* const M,
const vector_t* const x, size_t dim0) {
    assert(M && x && M->data && x->data);

    /* choose appropriate multiplication routine */
    switch (M->sparsityType) {
        case QPDUNES_DIAGONAL:
            return multiplyVectorMatrixVectorDiagonal(M->data, x->data, dim0);
        case QPDUNES_IDENTITY:
            /* just square vector */
            return scalarProd(x, x, dim0);
        case QPDUNES_DENSE:
        case QPDUNES_SPARSE:
        case QPDUNES_MATRIX_UNDEFINED:
        case QPDUNES_ALLZEROS:
            assert(0 && "Invalid M sparsity type");
            return -1.0;
    }
}


/*
Matrix-vector product res = invM*x, using a Cholesky factorization H = L*L^T,
where L is a lower triangular matrix.

Solve L*L^T * res = x for res by
1) solving L*y = x for y
2) solving L^T*res = y for res

dim0 is the dimension of the symmetric matrix.
*/
return_t multiplyInvMatrixVector(qpData_t* const qpData, vector_t* const res,
const matrix_t* const cholH, const vector_t* const x, size_t dim0) {
    assert(qpData && res && cholH && x && res->data && cholH->data &&
           x->data);

    /* choose appropriate multiplication routine */
    switch (cholH->sparsityType) {
        case QPDUNES_DIAGONAL:
            /*
            cholH in this case contains full diagonal matrix (not a factor)
            */
            return backsolveDiagonal(qpData, res->data, cholH->data, x->data,
                                     dim0);
        case QPDUNES_IDENTITY:
            /* just copy vector */
            qpDUNES_copyVector(res, x, dim0);
            return QPDUNES_OK;
        case QPDUNES_DENSE:
        case QPDUNES_SPARSE:
        case QPDUNES_MATRIX_UNDEFINED:
        case QPDUNES_ALLZEROS:
            assert(0 && "Invalid cholH sparsity type");
            return QPDUNES_ERR_UNKNOWN_MATRIX_SPARSITY_TYPE;
    }
}


/* Generic diagonal vector-matrix-vector product a = x'*M*x */
real_t multiplyVectorMatrixVectorDiagonal(const real_t* const M,
const real_t* const x, size_t dim0) {
    assert(M && x);
    _nassert((size_t)M % 4 == 0);
    _nassert((size_t)x % 4 == 0);

    size_t j;
    real_t result = 0.;

    /* multiply vector with diagonal matrix saved in first line */
    for (j = 0; j < dim0; j++) {
        result += accM(0, j, dim0) * x[j] * x[j];
    }

    return result;
}


/* Low level scalar product */
real_t scalarProd(const vector_t* const x, const vector_t* const y,
size_t len) {
    assert(x && y);
    _nassert((size_t)x % 4 == 0);
    _nassert((size_t)y % 4 == 0);

    size_t i;
    real_t res = 0.0;

    for (i = 0; i < len; i++) {
        res += x->data[i] * y->data[i];
    }

    return res;
}


real_t vectorNorm(const vector_t* const vec, size_t len) {
    return sqrt_f(scalarProd(vec, vec, len));
}
