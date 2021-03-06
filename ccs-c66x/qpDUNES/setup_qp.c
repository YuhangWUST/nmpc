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
 *	\file src/dual_qp.c
 *	\author Janick Frasch, Hans Joachim Ferreau
 *	\version 1.0beta
 *	\date 2012
 */


#include "dual_qp.h"


/* ----------------------------------------------
 *
#>>>>>>                                           */
void qpDUNES_indicateDataChange(	qpData_t* const qpData
									)
{
	int_t kk, ii;

	/* initialize prevIeqStatus to safe values when data was changed to force Hessian refactorization */
	for( kk=0; kk<_NI_+1; ++kk ) {
		for( ii=0; ii<_ND(kk)+_NV(kk); ++ii ) {
			qpData->log.itLog[0].prevIeqStatus[kk][ii] = -42;			/* some safe dummy value */
		}
	}
}
/*<<< END OF qpDUNES_indicateDataChange */


/* ----------------------------------------------
 *
 >>>>>>                                           */
return_t qpDUNES_updateData(	qpData_t* const qpData,
								const real_t* const H_,
								const real_t* const g_,
								const real_t* const C_,
								const real_t* const c_,
								const real_t* const zLow_,
								const real_t* const zUpp_,
								const real_t* const D_,
								const real_t* const dLow_,
								const real_t* const dUpp_
								)
{
	int_t kk;

	int_t nDoffset = 0;

	/** setup regular intervals */
	for( kk=0; kk<_NI_; ++kk )
	{
		qpDUNES_updateIntervalData( qpData, qpData->intervals[kk],
									 offsetArray(H_, kk*_NZ_*_NZ_), offsetArray(g_, kk*_NZ_),
									 offsetArray(C_, kk*_NX_*_NZ_), offsetArray(c_, kk*_NX_),
									 offsetArray(zLow_, kk*_NZ_), offsetArray(zUpp_, kk*_NZ_),
									 offsetArray(D_, nDoffset*_NZ_), offsetArray(dLow_, nDoffset), offsetArray(dUpp_, nDoffset),
									 0 );
		nDoffset += qpData->intervals[kk]->nD;
	}
	/** set up final interval */
	qpDUNES_updateIntervalData( qpData, qpData->intervals[_NI_],
							 offsetArray(H_, _NI_*_NZ_*_NZ_), offsetArray(g_, _NI_*_NZ_),
							 0, 0,
							 offsetArray(zLow_, _NI_*_NZ_), offsetArray(zUpp_, _NI_*_NZ_),
							 offsetArray(D_, nDoffset*_NZ_), offsetArray(dLow_, nDoffset), offsetArray(dUpp_, nDoffset),
							 0 );

	/* reset current active set to force Hessian refactorization
	 * (needed if matrix data entering the Newton Hessian has changed) */
	if ( (H_ !=0) || (C_ != 0) || (D_ != 0) ) {
		qpDUNES_indicateDataChange(qpData);
	}

	return QPDUNES_OK;
}
/*<<< END OF qpDUNES_updateData */


/* ----------------------------------------------
 * data setup function
 *
 >>>>>>                                           */
return_t qpDUNES_setupRegularInterval(	qpData_t* const qpData,
									interval_t* interval,
									const real_t* const H_,
									const real_t* const Q_,
									const real_t* const R_,
									const real_t* const S_,
									const real_t* const g_,
									const real_t* const C_,
									const real_t* const A_,
									const real_t* const B_,
									const real_t* const c_,
									const real_t* const zLow_,
									const real_t* const zUpp_,
									const real_t* const xLow_,
									const real_t* const xUpp_,
									const real_t* const uLow_,
									const real_t* const uUpp_,
									const real_t* const D_,
									const real_t* const dLow_,
									const real_t* const dUpp_
									)
{
	int_t ii, jj;

	int_t nD = interval->nD;	/* TODO: enable ND static for full static memory!*/
	int_t nV = interval->nV;

	vv_matrix_t* H = &(interval->H);
	xz_matrix_t* C = &(interval->C);

	sparsityType_t sparsityQ;
	sparsityType_t sparsityR;

	/** (1) quadratic term of cost function */
	if ( H_ != 0 ) {	/* Hessian given directly */
		if (H->sparsityType == QPDUNES_MATRIX_UNDEFINED) {
			H->sparsityType = qpDUNES_detectMatrixSparsity( H_, _NZ_, _NZ_ );
		}
		qpDUNES_updateMatrixData( (matrix_t*)H, H_, _NZ_, _NZ_ );
	}
	else {	/* assemble Hessian */
		/* TODO: move Q, R out to MPC module */
		/* detect sparsity of Q, R */
		sparsityQ =  (Q_ != 0) ? qpDUNES_detectMatrixSparsity( Q_, _NX_, _NX_ ) : QPDUNES_IDENTITY;
		sparsityR =  (R_ != 0) ? qpDUNES_detectMatrixSparsity( R_, _NU_, _NU_ ) : QPDUNES_IDENTITY;

		if ( S_ != 0 ) {	/* assemble full (dense) Hessian */
			H->sparsityType = QPDUNES_DENSE;
			/* Hessian written completely; TODO: check if one triangular half Hessian would be sufficient */
			for ( ii=0; ii<_NX_; ++ii ) {
				if ( Q_ != 0 ) {			/* Q part */
					for( jj=0; jj<_NX_; ++jj ) {
						accH( ii,jj ) = Q_[ii*_NX_+jj];
					}
				}
				else {
					accH( ii,ii ) = qpData->options.regParam;
				}
				for( jj=0; jj<_NU_; ++jj ) {	/* S part */
					accH( ii,_NX_+jj ) = S_[ii*_NU_+jj];
				}
			}
			for ( ii=0; ii<_NU_; ++ii ) {
				for( jj=0; jj<_NX_; ++jj ) {	/* S^T part */
					accH( _NX_+ii,jj ) = S_[jj*_NX_+ii];
				}
				if ( R_ != 0 ) {			/* R part */
					for( jj=0; jj<_NU_; ++jj ) {
						accH( _NX_+ii,_NX_+jj ) = R_[ii*_NU_+jj];
					}
				}
				else {
					accH( _NX_+ii,_NX_+ii ) = qpData->options.regParam;
				}
			}
		}
		else {	/* write Hessian blocks */
			if ( (sparsityQ == QPDUNES_DENSE) || (sparsityR == QPDUNES_DENSE) ) {
				H->sparsityType = QPDUNES_DENSE;
				for ( ii=0; ii<_NX_; ++ii ) {
					/* Q part */
					if ( Q_ != 0 ) {
						for( jj=0; jj<_NX_; ++jj ) {
							accH( ii,jj ) = Q_[ii*_NX_+jj];
						}
					}
					else {
						for( jj=0; jj<ii; ++jj ) {
							accH( ii,jj ) = 0.;
						}
						accH( ii,ii ) = qpData->options.regParam;
						for( jj=ii+1; jj<_NX_; ++jj ) {
							accH( ii,jj ) = 0.;
						}
					}
					/* S part */
					for( jj=_NX_; jj<_NZ_; ++jj ) {
						accH( ii,jj ) = 0.;
					}
				}
				for ( ii=0; ii<_NU_; ++ii ) {
					/* S^T part */
					for( jj=0; jj<_NX_; ++jj ) {
						accH( _NX_+ii,jj ) = 0.;
					}
					/* R part */
					if ( R_ != 0 ) {
						for( jj=0; jj<_NU_; ++jj ) {
							accH( _NX_+ii,_NX_+jj ) = R_[ii*_NU_+jj];
						}
					}
					else {
						for( jj=0; jj<ii; ++jj ) {
							accH( _NX_+ii,_NX_+jj ) = 0.;
						}
						accH( _NX_+ii,_NX_+ii ) = qpData->options.regParam;
						for( jj=ii+1; jj<_NX_; ++jj ) {
							accH( _NX_+ii,_NX_+jj ) = 0.;
						}
					}
				}
			}
			else {	/* Q and R block are diagonal or identity */
				if ( (sparsityQ == QPDUNES_IDENTITY) && (sparsityR == QPDUNES_IDENTITY) ) {
					H->sparsityType = QPDUNES_IDENTITY;
					/* no data needs to be written */
				}
				else {
					H->sparsityType = QPDUNES_DIAGONAL;
					/* write diagonal in first line for cache efficiency */
					/* Q part */
					if (sparsityQ == QPDUNES_IDENTITY) {
						for( ii=0; ii<_NX_; ++ii) {
							accH( 0,ii ) = 1.;
						}
					}
					else {
						for( ii=0; ii<_NX_; ++ii) {
							accH( 0,ii ) = Q_[ii*_NX_+ii];
						}
					}
					/* R part */
					if (sparsityR == QPDUNES_IDENTITY) {
						for( ii=0; ii<_NU_; ++ii) {
							accH( 0,_NX_+ii ) = 1.;
						}
					}
					else {
						for( ii=0; ii<_NU_; ++ii) {
							accH( 0,_NX_+ii ) = R_[ii*_NU_+ii];
						}
					}
				}
			}
		} /* end of write Hessian blocks */
	} /* end of Hessian */

	if (H->sparsityType  < QPDUNES_DIAGONAL) {
		return QPDUNES_ERR_INVALID_ARGUMENT;
	}


	/** (2) linear term of cost function */
	if ( g_ != 0 ) {
		qpDUNES_setupVector( (vector_t*)&(interval->g), g_, nV );
	}
	else {
		qpDUNES_setupZeroVector( (vector_t*)&(interval->g), nV );
	}


	/** (3) dynamic system */
	if (C->sparsityType == QPDUNES_MATRIX_UNDEFINED) {
		C->sparsityType = QPDUNES_DENSE;
	}
	if ( C_ != 0 ) {
		/* set up C directly */
		qpDUNES_updateMatrixData( (matrix_t*)C, C_, _NX_, _NZ_ );
	}
	else {
		/* TODO: move assembly out to MPC interface */
		if ( (A_ != 0) && (B_ != 0) ) {
			/* build up C */
			for ( ii=0; ii<_NX_; ++ii ) {
				for( jj=0; jj<_NX_; ++jj ) {
					accC( ii, jj ) = A_[ii*_NX_+jj];
				}
				for( jj=0; jj<_NU_; ++jj ) {
					accC( ii, _NX_+jj ) = B_[ii*_NU_+jj];
				}
			}
		}
		else {
			return QPDUNES_ERR_INVALID_ARGUMENT;
		}
	}

	if ( c_ != 0 ) {
		qpDUNES_setupVector( (vector_t*)&(interval->c), c_, _NX_ );
	}
	else {
		qpDUNES_setupZeroVector( (vector_t*)&(interval->c), _NX_ );
	}


	/** (4) bounds */
	qpDUNES_setupUniformVector( (vector_t*)&(interval->zLow), -qpData->options.QPDUNES_INFTY, nV );
	qpDUNES_updateSimpleBoundVector( qpData, (vector_t*)&(interval->zLow), zLow_, xLow_, uLow_ );
	qpDUNES_setupUniformVector( (vector_t*)&(interval->zUpp), qpData->options.QPDUNES_INFTY, nV );
	qpDUNES_updateSimpleBoundVector( qpData, (vector_t*)&(interval->zUpp), zUpp_, xUpp_, uUpp_ );


	/** (5) constraints */
	/*  - Matrix */
	if ( D_ != 0 ) {	/* generically bounded QP */
		if (interval->D.sparsityType == QPDUNES_MATRIX_UNDEFINED) {
			interval->D.sparsityType = qpDUNES_detectMatrixSparsity( D_, nD, _NZ_ );
		}
		qpDUNES_updateMatrixData( (matrix_t*)&(interval->D), D_, nD, _NZ_ );

	}
	else {	/* simply bounded QP */
		qpDUNES_setMatrixNull( (matrix_t*)&(interval->D) );
	}

	/*  - Vectors */
	qpDUNES_updateVector( (vector_t*)&(interval->dLow), dLow_, nD );
	qpDUNES_updateVector( (vector_t*)&(interval->dUpp), dUpp_, nD );


	return QPDUNES_OK;
}
/*<<< END OF qpDUNES_setupRegularInterval */



/* ----------------------------------------------
 * data setup function
 *
 >>>>>>                                           */
return_t qpDUNES_setupFinalInterval(	qpData_t* const qpData,
									interval_t* interval,
 									const real_t* const H_,
 									const real_t* const g_,
									const real_t* const zLow_,
									const real_t* const zUpp_,
									const real_t* const D_,
									const real_t* const dLow_,
									const real_t* const dUpp_
									)
{
	int_t nV = interval->nV;
	int_t nD = interval->nD;

	vv_matrix_t* H = &(interval->H);


	/** (1) quadratic term of cost function */
 	if ( H_ != 0 ) {	/* H given */
 		H->sparsityType = qpDUNES_detectMatrixSparsity( H_, nV, nV );
 		qpDUNES_updateMatrixData( (matrix_t*)H, H_, nV, nV );
 	}
	else {
		qpDUNES_setupScaledIdentityMatrix( _NX_, qpData->options.regParam, (matrix_t*)H );
	}


 	/** (2) linear term of cost function */
	if ( g_ != 0 ) {
		qpDUNES_setupVector( (vector_t*)&(interval->g), g_, nV );
	}
	else {
		qpDUNES_setupZeroVector( (vector_t*)&(interval->g), nV );
	}


	/** (3) local bounds */
	qpDUNES_setupUniformVector( (vector_t*)&(interval->zLow), -qpData->options.QPDUNES_INFTY, nV );
	qpDUNES_updateVector( (vector_t*)&(interval->zLow), zLow_, nV );
	qpDUNES_setupUniformVector( (vector_t*)&(interval->zUpp), qpData->options.QPDUNES_INFTY, nV );
	qpDUNES_updateVector( (vector_t*)&(interval->zUpp), zUpp_, nV );


	/** (4) local constraints */
	if ( D_ != 0 ) {	/* generically bounded QP */
		if (interval->D.sparsityType == QPDUNES_MATRIX_UNDEFINED) {
			interval->D.sparsityType = qpDUNES_detectMatrixSparsity( D_, nD, nV );
		}
		qpDUNES_updateMatrixData( (matrix_t*)&(interval->D), D_, nD, nV );
	}
	else {	/* simply bounded QP */
		qpDUNES_setMatrixNull( (matrix_t*)&(interval->D) );
	}

	qpDUNES_updateVector( (vector_t*)&(interval->dLow), dLow_, nD );
	qpDUNES_updateVector( (vector_t*)&(interval->dUpp), dUpp_, nD );

	return QPDUNES_OK;
}
/*<<< END OF qpDUNES_setupIntervalData */




/* ----------------------------------------------
 * data update function
 *
 >>>>>>                                           */
return_t qpDUNES_updateIntervalData(	qpData_t* const qpData,
										interval_t* interval,
										const real_t* const H_,
										const real_t* const g_,
										const real_t* const C_,
										const real_t* const c_,
										const real_t* const zLow_,
										const real_t* const zUpp_,
										const real_t* const D_,
										const real_t* const dLow_,
										const real_t* const dUpp_,
										vv_matrix_t* const cholH
										)
{
	int_t nD = interval->nD;


	int_t nV = interval->nV;

	boolean_t refactorHessian;


	/** copy data */
	qpDUNES_updateMatrixData( (matrix_t*)&(interval->H), H_, nV, nV );
	qpDUNES_updateVector( (vector_t*)&(interval->g), g_, nV );

	qpDUNES_updateMatrixData( (matrix_t*)&(interval->C), C_, _NX_, _NZ_ );
	qpDUNES_updateVector( (vector_t*)&(interval->c), c_, _NX_ );

	qpDUNES_updateVector( (vector_t*)&(interval->zLow), zLow_, nV );
	qpDUNES_updateVector( (vector_t*)&(interval->zUpp), zUpp_, nV );

	/* generically bounded QP */
	if ( D_ != 0 ) {
		qpDUNES_updateMatrixData( (matrix_t*)&(interval->D), D_, nD, nV );
	}
	qpDUNES_updateVector( (vector_t*)&(interval->dLow), dLow_, nD );
	qpDUNES_updateVector( (vector_t*)&(interval->dUpp), dUpp_, nD );


	/** re-factorize Hessian for direct QP solver if needed */
	/** re-run stage QP setup if objective and/or matrices changed */
	if ( (H_ != 0) || (g_ != 0) || (D_ != 0) ) 	/* matrices and/or QP objective were changed */
	{
		refactorHessian = QPDUNES_FALSE;
		if (H_ != 0) {			/* updated H */
			if (cholH != 0) {	/* factorization provided */
				qpDUNES_copyMatrix( (matrix_t*)&(interval->cholH), (matrix_t*)cholH, nV, nV );
			}
		else {					/* no factorization provided */
				refactorHessian = QPDUNES_TRUE;
			}
		}

		qpDUNES_setupStageQP( qpData, interval, refactorHessian );
	}


	return QPDUNES_OK;
}
/*<<< END OF qpDUNES_updateIntervalData */




/* ----------------------------------------------
 * set up of local QP
 *
 >>>>>>                                           */
return_t qpDUNES_setupAllLocalQPs(	qpData_t* const qpData,
								boolean_t isLTI
								)
{
	int_t kk;
	interval_t* interval;

	boolean_t refactorHessian;


	/* (1) set up initial lambda guess */
	qpDUNES_updateVector( &(qpData->intervals[0]->lambdaK1), &(qpData->lambda.data[0]), _NX_ );
	for( kk=1; kk<_NI_; ++kk ) {
		qpDUNES_updateVector( &(qpData->intervals[kk]->lambdaK), &(qpData->lambda.data[(kk-1)*_NX_]), _NX_ );
		qpDUNES_updateVector( &(qpData->intervals[kk]->lambdaK1), &(qpData->lambda.data[kk*_NX_]), _NX_ );
	}
	qpDUNES_updateVector( &(qpData->intervals[_NI_]->lambdaK), &(qpData->lambda.data[(_NI_-1)*_NX_]), _NX_ );


	/* (2) decide which QP solver to use and set up */
	for( kk=0; kk<_NI_+1; ++kk ) {
		interval = qpData->intervals[kk];

		/* (a) decide which stage QP solver to use */
		if ( interval->qpSolverSpecification == QPDUNES_STAGE_QP_SOLVER_UNDEFINED )
		{
			if ( ( interval->H.sparsityType >= QPDUNES_DIAGONAL ) &&
				 ( interval->nD == 0 ) )
			{
				interval->qpSolverSpecification = QPDUNES_STAGE_QP_SOLVER_CLIPPING;
			}
			else	{
				interval->qpSolverSpecification = QPDUNES_STAGE_QP_SOLVER_QPOASES;
			}
		}


		/* (b) copy cholH in LTI case for efficiency */
		refactorHessian = QPDUNES_TRUE;

		if ( ( interval->qpSolverSpecification == QPDUNES_STAGE_QP_SOLVER_CLIPPING ) &&
			 (isLTI) && (kk != 0) && (kk != _NI_) )	{
			/* only first Hessian needs to be factorized in LTI case, others can be copied;
			 * last one might still be different, due to terminal cost, even in LTI case */
			qpDUNES_copyMatrix( &(interval->cholH), &(qpData->intervals[0]->cholH), interval->nV, interval->nV );

			refactorHessian = QPDUNES_FALSE;
		}


		/* (c) prepare stage QP solvers */
		qpDUNES_setupStageQP( qpData, interval, refactorHessian );

	}

	return QPDUNES_OK;
}
/*<<< END OF qpDUNES_setupAllLocalQPs */


/* ----------------------------------------------
 *
 >>>>>>                                           */
return_t qpDUNES_setupStageQP(	qpData_t* const qpData,
								interval_t* const interval,
								boolean_t refactorHessian
								)
{
	return_t statusFlag;


	/* presolve first QP */
	if ( interval->qpSolverSpecification == QPDUNES_STAGE_QP_SOLVER_CLIPPING ) {
		/* (a) use clipping stage QP solver */
		interval->qpSolverSpecification = QPDUNES_STAGE_QP_SOLVER_CLIPPING;

		/* (b) prepare clipping QP solver */
		if ( refactorHessian == QPDUNES_TRUE ) {	/* only first Hessian needs to be factorized in LTI case, others can be copied; last one might still be different, due to terminal cost, even in LTI case */
			factorizeH( qpData, &(interval->cholH), &(interval->H), interval->nV );
		}

		/* (c) solve unconstrained local QP for g and initial lambda guess: */
		/*	   - get (possibly updated) lambda guess */
		if (interval->id > 0) {		/* lambdaK exists */
			qpDUNES_updateVector( &(interval->lambdaK), &(qpData->lambda.data[((interval->id)-1)*_NX_]), _NX_ );
		}
		if (interval->id < _NI_) {		/* lambdaK1 exists */
			qpDUNES_updateVector( &(interval->lambdaK1), &(qpData->lambda.data[(interval->id)*_NX_]), _NX_ );
		}

		/*     - update first order term */
		qpDUNES_setupZeroVector( &(interval->q), interval->nV );	/* reset q; qStep is added in qpDUNES_solve, when bounds are known */
		clippingQpSolver_updateStageData( qpData, interval, &(interval->lambdaK), &(interval->lambdaK1) );
		addToVector( &(interval->qpSolverClipping.qStep), &(interval->g), interval->nV );	/* Note: qStep is rewritten in line before */
		/*     - solve */
		statusFlag = directQpSolver_solveUnconstrained( qpData, interval, &(interval->qpSolverClipping.qStep) );
		if ( statusFlag != QPDUNES_OK ) {
			return statusFlag;
		}

		qpDUNES_setupZeroVector( &(interval->qpSolverClipping.zUnconstrained), interval->nV );	/* reset zUnconstrained */
	}
	else
	{
		/* (a) use qpOASES */
		interval->qpSolverSpecification = QPDUNES_STAGE_QP_SOLVER_QPOASES;

		/* (b) prepare first order term: initial lambda guess and g */
		/*	   - get primal first order term */
		qpDUNES_copyVector( &(interval->q), &(interval->g), interval->nV );
		/*	   - get (possibly updated) lambda guess */
		if (interval->id > 0) {		/* lambdaK exists */
			qpDUNES_updateVector( &(interval->lambdaK), &(qpData->lambda.data[((interval->id)-1)*_NX_]), _NX_ );
		}
		if (interval->id < _NI_) {		/* lambdaK1 exists */
			qpDUNES_updateVector( &(interval->lambdaK1), &(qpData->lambda.data[(interval->id)*_NX_]), _NX_ );
		}
		qpOASES_updateStageData( qpData, interval, &(interval->lambdaK), &(interval->lambdaK1) );

		/* (c) initialize qpOASES and run initial homotopy (i.e., solve first QP) */
		statusFlag = qpOASES_setup( qpData, interval->qpSolverQpoases.qpoasesObject, interval,
									&(interval->H), &(interval->qpSolverQpoases.qFullStep), /*&(interval->g),*/
									&(interval->zLow), &(interval->zUpp),
									&(interval->D), &(interval->dLow), &(interval->dUpp));
	}

	return statusFlag;
}
/*<<< END OF qpDUNES_setupStageQP */



/* ----------------------------------------------
 *
 >>>>>>                                           */
return_t qpDUNES_shiftIntervals(	qpData_t* const qpData
								)
{
	int_t kk;

	/** (1) Shift Interval pointers */
	/*  save pointer to first interval */
	interval_t* freeInterval = qpData->intervals[0];

	/*  shift all but the last interval (different size) left */
	for (kk=0; kk<_NI_-1; ++kk) {
		qpData->intervals[kk] = qpData->intervals[kk+1];
		qpData->intervals[kk]->id = kk;			/* correct stage index */
	}
	/*  hang the free interval on the second but last position */
	qpData->intervals[_NI_-1] = freeInterval;
	qpData->intervals[_NI_-1]->id = _NI_-1;		/* correct stage index */

	/* update definedness of lambda parts */
	qpData->intervals[0]->lambdaK.isDefined = QPDUNES_FALSE;
	qpData->intervals[_NI_-1]->lambdaK.isDefined = QPDUNES_TRUE;

	return QPDUNES_OK;
}
/*<<< END OF qpDUNES_shiftIntervals */



/* ----------------------------------------------
 *
 >>>>>>                                           */
return_t qpDUNES_shiftLambda(	qpData_t* const qpData
							)
{
	int_t kk, ii;

	for (kk=0; kk<_NI_-1; ++kk) {
		for (ii=0; ii<_NX_; ++ii) {
			qpData->lambda.data[kk*_NX_+ii] = qpData->lambda.data[(kk+1)*_NX_+ii];
		}
	}

	return QPDUNES_OK;
}
/*<<< END OF qpDUNES_shiftLambda */



qpOptions_t qpDUNES_setupDefaultOptions(
										)
{
	qpOptions_t options;

	/* iteration limits */
	options.maxIter               		= 100;
	options.maxNumLineSearchIterations 	= 19;				/* 0.3^19 = 1e-10 */
	options.maxNumLineSearchRefinementIterations 	= 40;	/* 0.62^49 = 1e-10 */

	/* printing */
	options.printLevel            		= 2;
	options.printIntervalHeader        	= 20;
	options.printIterationTiming		= QPDUNES_FALSE;
	options.printLineSearchTiming		= QPDUNES_FALSE;

	/* logging */
	options.logLevel            		= QPDUNES_LOG_OFF;

	/* numerical tolerances */
	options.stationarityTolerance 		= 1.e-6;
	options.equalityTolerance     		= 2.221e-16;
	options.newtonHessDiagRegTolerance  = 1.e-10;
	options.activenessTolerance			= 1e4*options.equalityTolerance;
	options.QPDUNES_ZERO             		= 1.e-20;
	options.QPDUNES_INFTY            		= 1.e12;
	options.ascentCurvatureTolerance	= 1.e-6;

	/* additional options */
	options.nbrInitialGradientSteps		= 0;
	options.checkForInfeasibility		= QPDUNES_FALSE;

	/* regularization option */
	options.regType 					= QPDUNES_REG_LEVENBERG_MARQUARDT;
	options.regParam			   		= 1.e-6;	/**< the regularization parameter added on singular Hessian elements
	 	 	 	 	 	 	 	 	 	 	 	 	 	 - should be quite a bit bigger than regularization tolerance
	 	 	 	 	 	 	 	 	 	 	 	 	 	 - assumption: if regularization needed, than Hessian has a singular direction
	 	 	 	 	 	 	 	 	 	 	 	 	 	 - in this singular direction i want to do mostly a gradient step,
	 	 	 	 	 	 	 	 	 	 	 	 	 	   few Hessian information usable
	 	 	 	 	 	 	 	 	 	 	 	 	  */

	options.nwtnHssnFacAlg				= QPDUNES_NH_FAC_BAND_REVERSE;


	/* line search options */
	options.lsType							= QPDUNES_LS_ACCELERATED_GRADIENT_BISECTION_LS;
	options.lineSearchReductionFactor		= 0.1;	/**< needs to be between 0 and 1 */
	options.lineSearchIncreaseFactor		= 1.5;	/**< needs to be greater than 1 */
	options.lineSearchMinAbsProgress    	= options.equalityTolerance;
	options.lineSearchMinRelProgress    	= 1.e-14;
	options.lineSearchStationarityTolerance = 1.e-3;
	options.lineSearchMaxStepSize   		= 1.;
	options.lineSearchNbrGridPoints   		= 5;

	/* qpOASES options */
	options.qpOASES_terminationTolerance	= 1.e-12;	/*< stationarity tolerance for qpOASES, see qpOASES::Options -> terminationTolerance */

	return options;
}
/*<<< END OF qpDUNES_setupOptions */

/*
 *	end of file
 */

