/*
 *	This file is part of qpDUNES.
 *
 *	qpDUNES -- A DUal NEwton Strategy for convex quadratic programming.
 *	Copyright (C) 2012 by Janick Frasch, Hans Joachim Ferreau et al.
 *	All rights reserved.
 *
 *	qpDUNES is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU Lesser General Public
 *	License as published by the Free Software Foundation; either
 *	version 2.1 of the License, or (at your option) any later version.
 *
 *	qpDUNES is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *	See the GNU Lesser General Public License for more details.
 *
 *	You should have received a copy of the GNU Lesser General Public
 *	License along with qpDUNES; if not, write to the Free Software
 *	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


/**
 *	\file include/qp/matrix_vector.h
 *	\author Janick Frasch, Hans Joachim Ferreau
 *	\version 1.0beta
 *	\date 2012
 */


#ifndef QP42_MATRIX_VECTOR_H
#define QP42_MATRIX_VECTOR_H


#include "types.h"
#include "utils.h"
#include <math.h>
#include <stddef.h>


real_t multiplyzHz(const vv_matrix_t* const H, const z_vector_t* const z,
const size_t nV);

return_t multiplyInvHz(qpData_t* const qpData, z_vector_t* const res,
const vv_matrix_t* const cholH, const z_vector_t* const z, const size_t nV);

return_t multiplyCz(qpData_t* const qpData, x_vector_t* const res,
const xz_matrix_t* const C, const z_vector_t* const z);

return_t multiplyCTy(qpData_t* const qpData, z_vector_t* const res,
const xz_matrix_t* const C, const x_vector_t* const y);

/** Inverse matrix times matrix product res = Q^-1 * A */
return_t multiplyAInvQ(qpData_t* const qpData, xx_matrix_t* const res,
const xx_matrix_t* const C, const vv_matrix_t* const cholH);

return_t getInvQ(qpData_t* const qpData, xx_matrix_t* const res,
const xx_matrix_t* const cholM1, size_t nV);

return_t factorizeH(qpData_t* const qpData, vv_matrix_t* const cholH,
const vv_matrix_t* const H, size_t nV);

return_t addScaledVector(xn_vector_t* const res, real_t scalingFactor,
const xn_vector_t* const update, size_t len);

return_t addVectorScaledVector(vector_t* const res, const vector_t* const x,
real_t scalingFactor, const vector_t* const y, size_t len);

return_t backsolveDenseL(qpData_t* const qpData, real_t* const res,
const real_t* const L, const real_t* const b, boolean_t transposed, size_t n);

return_t backsolveDiagonal(qpData_t* const qpData, real_t* const res,
const real_t* const M, const real_t* const b, size_t n);

return_t backsolveMatrixDiagonalIdentity(qpData_t* const qpData,
real_t* const res, const real_t* const M1, size_t dim0);

return_t factorizePosDefMatrix(qpData_t* const qpData, matrix_t* const cholM,
const matrix_t* const M, size_t dim0);

return_t denseCholeskyFactorization(qpData_t* const qpData,
matrix_t* const cholM, const matrix_t* const M, size_t dim0);

/* Matrix backsolve for L, M both dense compute res for L*res = M^T */
return_t backsolveMatrixTDenseDenseL(qpData_t* const qpData,
real_t* const res, const real_t* const L,
const real_t* const M, /* untransposed M */
real_t* const sums, /* memory for saving intermediate results (for speedup) */
boolean_t transposedL,
size_t dim0, size_t dim1); /* dimensions of M */

return_t backsolveMatrixTDiagonalDense(qpData_t* const qpData,
real_t* const res, const real_t* const M1,
const real_t* const M2,	/* untransposed M2 */
size_t dim0, size_t dim1); /* dimensions of M2 */

return_t multiplyMatrixVector(vector_t* const res, const matrix_t* const M,
const vector_t* const x, size_t dim0, size_t dim1);

real_t multiplyVectorMatrixVector(const matrix_t* const M,
const vector_t* const x, size_t dim0);

return_t multiplyInvMatrixVector(qpData_t* const qpData, vector_t* const res,
const matrix_t* const cholH, const vector_t* const x,
size_t dim0); /* dimension of symmetric matrix */

return_t multiplyMatrixVectorDense(real_t* const res, const real_t* const M,
const real_t* const x, size_t dim0, size_t dim1);

real_t multiplyVectorMatrixVectorDense(const real_t* const M,
const real_t* const x, size_t dim0);

return_t multiplyMatrixVectorSparse(real_t* const res, const real_t* const M,
const real_t* const x, size_t dim0, size_t dim1);

return_t multiplyMatrixVectorDiagonal(real_t* const res,
const real_t* const M, const real_t* const x, size_t dim0);

real_t multiplyVectorMatrixVectorDiagonal(const real_t* const M,
const real_t* const x, size_t dim0	);

/** Low-level scalar product */
real_t scalarProd(const vector_t* const x, const vector_t* const y,
size_t len);

return_t addToVector(vector_t* const res, const vector_t* const update,
size_t len);

return_t subtractFromVector(vector_t* const res, const vector_t* const update,
size_t len);

return_t negateVector(vector_t* const res, size_t len);

real_t vectorNorm(const vector_t* const vec, size_t len);

return_t addCInvHCT(qpData_t* const qpData, xx_matrix_t* const res,
const vv_matrix_t* const cholH, const xz_matrix_t* const C,
const d2_vector_t* const y, zx_matrix_t* const zxMatTmp);

return_t addMultiplyMatrixInvMatrixMatrixT(qpData_t* const qpData,
matrix_t* const res, const matrix_t* const cholM1, const matrix_t* const M2,
const real_t* const y, /* vector containing non-zeros for columns of M2 to be eliminated */
matrix_t* const Ztmp, /* temporary matrix of shape dim1 x dim0 */
vector_t* const vecTmp,
size_t dim0, size_t dim1); /* dimensions of M2 */

#endif	/* QP42_MATRIX_VECTOR_H */
