//////////////////////////////////////////////////////////////////////
//
//                             Pixie
//
// Copyright � 1999 - 2003, Okan Arikan
//
// Contact: okan@cs.berkeley.edu
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
// 
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
// 
// You should have received a copy of the GNU General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
//
//  File				:	algebra.h
//  Description			:	This file defines some math
//
////////////////////////////////////////////////////////////////////////
#ifndef ALGEBRA_H
#define ALGEBRA_H

#include <math.h>

#include "global.h"




////////////////////////////////////////////////////////////////////////////
//
//
//
//	Low precision routines are not available on all platforms
//
//
//
////////////////////////////////////////////////////////////////////////////
#ifndef sqrtf
#define	sqrtf	(float) sqrt
#endif

#ifndef cosf
#define	cosf	(float) cos
#endif

#ifndef sinf
#define	sinf	(float) sin
#endif

#ifndef tanf
#define	tanf	(float) tan
#endif

#ifndef atan2f
#define	atan2f	(float) atan2
#endif

#ifndef powf
#define	powf	(float) pow
#endif

#ifndef logf
#define	logf	(float) log
#endif

#ifndef fmodf
#define	fmodf	(float) fmod
#endif



















////////////////////////////////////////////////////////////////////////////
//
//
//
//	Matrix - vector	types
//
//
//
////////////////////////////////////////////////////////////////////////////
typedef float	vector[3];									// an array of 3 floats
typedef float	quaternion[4];								// an array of 4 floats
typedef float	htpoint[4];									// an array of 4 floats
typedef float	matrix[16];									// an array of 16 floats

typedef double	dvector[3];									// an array of 3 doubles
typedef double	dquaternion[4];								// an array of 4 doubles
typedef float	dhtpoint[4];								// an array of 4 floats
typedef double	dmatrix[16];								// an array of 16 doubles

// Row major matrix element order (compatible with openGL)
#define	element(row,column)	(row+(column<<2))


#define COMP_X	0
#define COMP_Y	1
#define COMP_Z	2

#define COMP_R	0
#define COMP_G	1
#define COMP_B	2









////////////////////////////////////////////////////////////////////////////
//
//
//
//	Faster versions of some math functions
//
//
//
////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////
// Fast inverse square root
inline	float	isqrtf(float number) {
	long		i;
	float		x2, y;
	const float threehalfs = 1.5F;

	x2 = number * 0.5F;
	y  = number;
	i  = * ( long * ) &y;
	i  = 0x5f3759df - ( i >> 1 );
	y  = * ( float * ) &i;
	y  = y * ( threehalfs - ( x2 * y * y ) );
	//	y  = y * ( threehalfs - ( x2 * y * y ) );			// Da second iteration

	return y;
}

////////////////////////////////////////////////////////////////////////////
// Fast floating point absolute value
inline	float	absf(float f) {
	int tmp = (*(int*)&f) & 0x7FFFFFFF;
	return *(float*)&tmp;
}




















////////////////////////////////////////////////////////////////////////////
//
//
//
//	Define some misc vector/matrix functions
//
//
//
////////////////////////////////////////////////////////////////////////////
#define SCALAR_TYPE	float
#define VECTOR_TYPE	vector
#define MATRIX_TYPE	matrix
#include "mathSpec.h"
#undef SCALAR_TYPE
#undef VECTOR_TYPE
#undef MATRIX_TYPE


#define SCALAR_TYPE	double
#define VECTOR_TYPE	dvector
#define MATRIX_TYPE	dmatrix
#include "mathSpec.h"
#undef SCALAR_TYPE
#undef VECTOR_TYPE
#undef MATRIX_TYPE



inline	void	normalizevf(float *r,const float *v) {
	const float	l	=	isqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);

	r[0]	=	(float) (v[0]*l);
	r[1]	=	(float) (v[1]*l);
	r[2]	=	(float) (v[2]*l);
}


inline	void	normalizevf(float *v) {
	const float	l	=	isqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);

	v[0]	=	(float) (v[0]*l);
	v[1]	=	(float) (v[1]*l);
	v[2]	=	(float) (v[2]*l);
}



















////////////////////////////////////////////////////////////////////////////
//
//
//
//	Misc polynomial solving. Defined with templates.
//
//
//
////////////////////////////////////////////////////////////////////////////
#define     cbrt(x)     ((x) > 0.0 ? pow(x, 1.0/3.0) : \
						((x) < 0.0 ? -pow(-x, 1.0/3.0) : 0.0))

template <class T> inline	int	solveQuadric(T a,T b,T c,T *r) {
	double	inva	=	1	/	a;
	double	nc		=	c*inva;
	double	nb		=	b*inva*0.5;
	double	delta	=	nb*nb - nc;

	if (delta < 0)	return	0;
	else {
		double	sqrtDelta	=	sqrt(delta);

		r[0]	=	(T) (-sqrtDelta - nb);
		r[1]	=	(T) (sqrtDelta - nb);

		return 2;
	}
}

template <class T> inline	int solveCubic(T c[4],T s[3]) {
    int     i, num;
    double	sub;
    double	A, B, C;
    double	sq_A, p, q;
    double	cb_p, D;
	double	sd[3];

    A		= c[2] / c[3];
    B		= c[1] / c[3];
    C		= c[0] / c[3];

    sq_A	= A * A;
    p		= (1.0/3 * (- 1.0/3 * sq_A + B));
    q		= (1.0/2 * (2.0/27 * A * sq_A - 1.0/3 * A * B + C));

    cb_p	= p * p * p;
    D		= q * q + cb_p;

    if (D == 0) {
		if (q == 0) {
			sd[0]		= 0;
			num			= 1;
		} else 	{
			double u	= (cbrt(-q));
			sd[0]		= 2 * u;
			sd[1]		= -u;
			num			= 2;
		}
    } else if (D < 0) {
		double phi		= (1.0/3 * acos(-q / sqrt(-cb_p)));
		double t		= (2 * sqrt(-p));

		sd[0]			=   t * cos(phi);
		sd[1]			= - t * cos(phi + C_PI / 3);
		sd[2]			= - t * cos(phi - C_PI / 3);
		num				= 3;
    } else {
		double sqrt_D	= sqrt(D);
		double u		= cbrt(sqrt_D - q);
		double v		= -cbrt(sqrt_D + q);

		sd[0]			= (u + v);
		num				= 1;
    }

    sub = (1.0/3 * A);

    for (i=0;i<num;++i)
		s[i] = (T) (sd[i] - sub);

    return num;
}


template <class T> inline	int solveQuartic(T c[5],T s[4]) {
    double		coeffs[4];
    double		z, u, v, sub;
    double		A, B, C, D;
    double		sq_A, p, q, r;
    int			i, num;
	double		sd[4];

    A = c[ 3 ] / c[ 4 ];
    B = c[ 2 ] / c[ 4 ];
    C = c[ 1 ] / c[ 4 ];
    D = c[ 0 ] / c[ 4 ];


    sq_A	= A * A;
    p		= (- 3.0/8 * sq_A + B);
    q		= (1.0/8 * sq_A * A - 1.0/2 * A * B + C);
    r		= (-3.0/256*sq_A*sq_A + 1.0/16*sq_A*B - 1.0/4*A*C + D);

    if (r == 0) {
		coeffs[ 0 ] = q;
		coeffs[ 1 ] = p;
		coeffs[ 2 ] = 0;
		coeffs[ 3 ] = 1;

		num			= solveCubic<double>(coeffs, sd);

		sd[num++]	= 0;
    } else {
		coeffs[ 0 ] = 1.0/2 * r * p - 1.0/8 * q * q;
		coeffs[ 1 ] = - r;
		coeffs[ 2 ] = - 1.0/2 * p;
		coeffs[ 3 ] = 1;

		solveCubic<double>(coeffs, sd);

		z = sd[ 0 ];

		u = z * z - r;
		v = 2 * z - p;

		if (u == 0)
			u = 0;
		else if (u > 0)
			u = sqrt(u);
		else
			return 0;

		if (v == 0)
			v = 0;
		else if (v > 0)
			v = sqrt(v);
		else
			return 0;

		coeffs[ 0 ] = z - u;
		coeffs[ 1 ] = q < 0 ? -v : v;
		coeffs[ 2 ] = 1;

		num = solveQuadric<double>(coeffs[2],coeffs[1],coeffs[0],sd);

		coeffs[ 0 ]= z + u;
		coeffs[ 1 ] = q < 0 ? v : -v;
		coeffs[ 2 ] = 1;

		num += solveQuadric<double>(coeffs[2],coeffs[1],coeffs[0],sd + num);
    }

    sub = 1.0/4 * A;

    for (i = 0; i < num; ++i) {
		s[i] = (T) (sd[i] - sub);
	}

    return num;
}







// Init/Shutdown for the math functions
void	mathInit();
void	mathShutdown();








#endif


