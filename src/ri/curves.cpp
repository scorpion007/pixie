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
//  File				:	curves.cpp
//  Classes				:	CCurve
//  Description			:	Curve primitive
//
////////////////////////////////////////////////////////////////////////
#include <math.h>

#include "curves.h"
#include "memory.h"
#include "stats.h"
#include "renderer.h"
#include "rendererContext.h"

// The inverse of the Bezier basis
static	matrix	invBezier	=	{	0,	0,				0,				1,
									0,	0,				1/(float) 3,	1,
									0,	1/(float) 3,	2/(float) 3,	1,
									1,	1,				1,				1};


///////////////////////////////////////////////////////////////////////
// Function				:	makeCubicBound
// Description			:	Converts the control vertices to Bezier control vertices
// Return Value			:	-
// Comments				:
// Date last edited		:	5/25/2004
static	inline	void	makeCubicBound(float *bmin,float *bmax,const float *v0,const float *v1,const float *v2,const float *v3,const float *geometryMatrix,const CXform *xform) {
	htpoint	tmp,tmp2;
	vector	vtmp0,vtmp1,vtmp2,vtmp3;

	tmp[0]			=	v0[COMP_X];
	tmp[1]			=	v1[COMP_X];
	tmp[2]			=	v2[COMP_X];
	tmp[3]			=	v3[COMP_X];
	mulpm4(tmp2,tmp,geometryMatrix);
	vtmp0[COMP_X]	=	tmp2[0];
	vtmp1[COMP_X]	=	tmp2[1];
	vtmp2[COMP_X]	=	tmp2[2];
	vtmp3[COMP_X]	=	tmp2[3];

	tmp[0]			=	v0[COMP_Y];
	tmp[1]			=	v1[COMP_Y];
	tmp[2]			=	v2[COMP_Y];
	tmp[3]			=	v3[COMP_Y];
	mulpm4(tmp2,tmp,geometryMatrix);
	vtmp0[COMP_Y]	=	tmp2[0];
	vtmp1[COMP_Y]	=	tmp2[1];
	vtmp2[COMP_Y]	=	tmp2[2];
	vtmp3[COMP_Y]	=	tmp2[3];

	tmp[0]			=	v0[COMP_Z];
	tmp[1]			=	v1[COMP_Z];
	tmp[2]			=	v2[COMP_Z];
	tmp[3]			=	v3[COMP_Z];
	mulpm4(tmp2,tmp,geometryMatrix);
	vtmp0[COMP_Z]	=	tmp2[0];
	vtmp1[COMP_Z]	=	tmp2[1];
	vtmp2[COMP_Z]	=	tmp2[2];
	vtmp3[COMP_Z]	=	tmp2[3];

	if (xform != NULL) {
		mulmp(vtmp0,xform->from,vtmp0);
		mulmp(vtmp1,xform->from,vtmp1);
		mulmp(vtmp2,xform->from,vtmp2);
		mulmp(vtmp3,xform->from,vtmp3);
	}

	addBox(bmin,bmax,vtmp0);
	addBox(bmin,bmax,vtmp1);
	addBox(bmin,bmax,vtmp2);
	addBox(bmin,bmax,vtmp3);
}


///////////////////////////////////////////////////////////////////////
// Class				:	CCurve
// Method				:	CCurve
// Description			:	Ctor
// Return Value			:	-
// Comments				:	
// Date last edited		:	6/2/2003
CCurve::CCurve(CAttributes *a,CXform *x,CBase *b,float vmi,float vma,float gvmi,float gvma) : CSurface(a,x) {
	stats.numGprims++;
	stats.gprimMemory	+=	sizeof(CCurve);

	vmin				=	vmi;
	vmax				=	vma;
	gvmin				=	gvmi;
	gvmax				=	gvma;
	base				=	b;
	base->attach();
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCurve
// Method				:	~CCurve
// Description			:	Dtor
// Return Value			:	-
// Comments				:
// Date last edited		:	6/2/2003
CCurve::~CCurve() {
	stats.numGprims--;
	stats.gprimMemory	-=	sizeof(CCurve);

	base->detach();
}


///////////////////////////////////////////////////////////////////////
// Class				:	CCubicCurve
// Method				:	interpolate
// Description			:	Interpolate the junk
// Return Value			:	-
// Comments				:
// Date last edited		:	6/2/2003
void			CCurve::interpolate(int numVertices,float **varying) const {

	// Dispatch the parameters
	if (base->parameters != NULL)	base->parameters->dispatch(numVertices,varying);

	// Normalize the v parameter
	float	*v	=	varying[VARIABLE_V];
	int		i;
	for (i=numVertices;i>0;i--)	*v++	=	(gvmax - gvmin)*v[0] + gvmin;

	// Get the width
	const float	*size;
	int			sizeStep;

	if (base->sizeVariable->entry == VARIABLE_WIDTH) {
		size		=	varying[VARIABLE_WIDTH];
		sizeStep	=	1;
	} else {
		assert(base->sizeVariable->entry == VARIABLE_CONSTANTWIDTH);
		size		=	varying[VARIABLE_CONSTANTWIDTH];
		sizeStep	=	0;
	}

	float		*dPdu	=	varying[VARIABLE_DPDU];
	float		*P		=	varying[VARIABLE_P];
	const float	*u		=	varying[VARIABLE_U];
	int			j;
	vector		tmp;
	for (j=numVertices;j>0;j--,P+=3,dPdu+=3,size+=sizeStep) {
		mulvf(tmp,dPdu,(*u++ - 0.5f)*size[0]);
		mulvf(dPdu,size[0]);
		addvv(P,tmp);
	}
}


///////////////////////////////////////////////////////////////////////
// Class				:	CCubicCurve
// Method				:	CCurve
// Description			:	Dice the curve group into smaller ones
// Return Value			:	-
// Comments				:
// Date last edited		:	6/2/2003
void			CCurve::dice(CShadingContext *rasterizer) {
	// We can sample the object, so do so
	float	**varying		=	rasterizer->currentShadingState->varying;
	float	*u				=	varying[VARIABLE_U];
	float	*v				=	varying[VARIABLE_V];

	// Sample 6 points on the curve

	// Top
	*v++	=	vmin;
	*u++	=	0;
	*v++	=	vmin;
	*u++	=	1;

	// Middle
	*v++	=	(vmin + vmax) * 0.5f;
	*u++	=	0;
	*v++	=	(vmin + vmax) * 0.5f;
	*u++	=	1;

	// Bottom
	*v++	=	vmax;
	*u++	=	0;
	*v++	=	vmax;
	*u++	=	1;

	// Sample the curves
	rasterizer->displace(this,2,3,SHADING_2D_GRID,PARAMETER_P | PARAMETER_BEGIN_SAMPLE);

	// Compute the curve bounding box
	float	*P		=	varying[VARIABLE_P];
	vector	bmin,bmax;
	initv(bmin,C_INFINITY,C_INFINITY,C_INFINITY);
	initv(bmax,-C_INFINITY,-C_INFINITY,-C_INFINITY);
	int		i;
	for (i=0;i<6;i++)	addBox(bmin,bmax,P + i*3);

	if (bmin[COMP_Z] < C_EPSILON) {
		if (bmax[COMP_Z] < CRenderer::clipMin) {
			// The curve is behind the screen

		} else if (CRenderer::inFrustrum(bmin,bmax) == FALSE) {
			// The curve is out of the viewing frustrum
			
		} else {
			// Split the curve into two pieces
			splitToChildren(rasterizer);
		}
	} else {
		// We can do the perspective division
		camera2pixels(6,P);

		// Estimate the dicing amount
		int		udiv,vdiv;
		estimateDicing(P,1,2,udiv,vdiv,attributes->shadingRate);

		// Make sure we don't split along u
		if (vdiv == 1)	udiv	=	min(udiv,(CRenderer::maxGridSize >> 1) - 1);

		// Can we render this sucker ?
		if ((udiv+1)*(vdiv+1) > CRenderer::maxGridSize) {
			splitToChildren(rasterizer);
		} else {
			rasterizer->drawGrid(this,udiv,vdiv,0,1,vmin,vmax);
		}
	}
}




























///////////////////////////////////////////////////////////////////////
// Class				:	CCubicCurve
// Method				:	CCubicCurve
// Description			:	Ctor
// Return Value			:	-
// Comments				:	
// Date last edited		:	6/2/2003
CCubicCurve::CCubicCurve(CAttributes *a,CXform *x,CBase *b,float vmi,float vma,float gvmi,float gvma) : CCurve(a,x,b,vmi,vma,gvmi,gvma) {

	// Compute the bounding box
	const CVertexData	*variables	=	base->variables;
	const int			vertexSize	=	variables->vertexSize;
	const int			vs			=	(variables->moving ? vertexSize*2 : vertexSize);
	const float			*vertex		=	base->vertex;
	const float			*v0			=	vertex;
	const float			*v1			=	v0 + vs;
	const float			*v2			=	v1 + vs;
	const float			*v3			=	v2 + vs;
	matrix				geometryMatrix;

	initv(bmin,C_INFINITY,C_INFINITY,C_INFINITY);
	initv(bmax,-C_INFINITY,-C_INFINITY,-C_INFINITY);

	mulmm(geometryMatrix,attributes->vBasis,invBezier);

	makeCubicBound(bmin,bmax,v0,v1,v2,v3,geometryMatrix,NULL);
	
	if (variables->moving) {
		v0	+=	vertexSize;
		v1	+=	vertexSize;
		v2	+=	vertexSize;
		v3	+=	vertexSize;

		makeCubicBound(bmin,bmax,v0,v1,v2,v3,geometryMatrix,NULL);
	}

	subvf(bmin,base->maxSize);
	addvf(bmax,base->maxSize);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCubicCurve
// Method				:	~CCubicCurve
// Description			:	Dtor
// Return Value			:	-
// Comments				:
// Date last edited		:	6/2/2003
CCubicCurve::~CCubicCurve() {
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCubicCurve
// Method				:	sample
// Description			:	Sample the curves
// Return Value			:	-
// Comments				:
// Date last edited		:	6/2/2003
void			CCubicCurve::sample(int start,int numVertices,float **varying,unsigned int &up) const {
	int				i,k;
	float			*intr,*intrStart;
	const	float	*vBasis				=	attributes->vBasis;
	CVertexData		*variables			=	base->variables;
	const	int		vertexSize			=	variables->vertexSize;
	const	int		vs					=	(variables->moving ? vertexSize*2 : vertexSize);
	const	float	*v0;
	const	float	*v1;
	const	float	*v2;
	const	float	*v3;

	intr	=	intrStart	=	(float *) alloca(numVertices*vertexSize*sizeof(float));

	if ((variables->moving == FALSE) || (up & PARAMETER_BEGIN_SAMPLE)) {
		v0		=	base->vertex;
		v1		=	v0 + vs;
		v2		=	v1 + vs;
		v3		=	v2 + vs;
	} else {
		v0		=	base->vertex + vertexSize;
		v1		=	v0 + vs;
		v2		=	v1 + vs;
		v3		=	v2 + vs;
	}

	const	float	*v					=	varying[VARIABLE_V] + start;
	for (i=numVertices;i>0;i--) {
		const	float	cv				=	*v++;
		float			vb[4];
		float			tmp[4];

		vb[3]	=	1;
		vb[2]	=	cv;
		vb[1]	=	cv*cv;
		vb[0]	=	cv*vb[1];

		tmp[0]	=	vb[0]*vBasis[element(0,0)] + vb[1]*vBasis[element(0,1)] + vb[2]*vBasis[element(0,2)] + vb[3]*vBasis[element(0,3)];
		tmp[1]	=	vb[0]*vBasis[element(1,0)] + vb[1]*vBasis[element(1,1)] + vb[2]*vBasis[element(1,2)] + vb[3]*vBasis[element(1,3)];
		tmp[2]	=	vb[0]*vBasis[element(2,0)] + vb[1]*vBasis[element(2,1)] + vb[2]*vBasis[element(2,2)] + vb[3]*vBasis[element(2,3)];
		tmp[3]	=	vb[0]*vBasis[element(3,0)] + vb[1]*vBasis[element(3,1)] + vb[2]*vBasis[element(3,2)] + vb[3]*vBasis[element(3,3)];

		*intr++	=	tmp[0]*v0[0] + tmp[1]*v1[0] + tmp[2]*v2[0] + tmp[3]*v3[0];
		*intr++	=	tmp[0]*v0[1] + tmp[1]*v1[1] + tmp[2]*v2[1] + tmp[3]*v3[1];
		*intr++	=	tmp[0]*v0[2] + tmp[1]*v1[2] + tmp[2]*v2[2] + tmp[3]*v3[2];

		for (k=3;k<vertexSize;k++) {
			*intr++	=	tmp[0]*v0[k] + tmp[1]*v1[k] + tmp[2]*v2[k] + tmp[3]*v3[k];
		}
	}

	// Dispatch the variables
	variables->dispatch(intrStart,0,numVertices,varying);

	float		*dPdv	=	varying[VARIABLE_DPDV]	+ start*3;
	float		*dPdu	=	varying[VARIABLE_DPDU]	+ start*3;
	const float	*P		=	varying[VARIABLE_P]		+ start*3;
	float		*N		=	varying[VARIABLE_NG]	+ start*3;

	v	=	varying[VARIABLE_V] + start*3;

	for (i=numVertices;i>0;i--,P+=3,dPdu+=3,dPdv+=3,N+=3) {
		const	float	cv	= *v++;
		float			vb[4];
		float			tmp[4];

		vb[3]	=	0;
		vb[2]	=	1;
		vb[1]	=	2*cv;
		vb[0]	=	3*cv*cv;

		tmp[0]	=	vb[0]*vBasis[element(0,0)] + vb[1]*vBasis[element(0,1)] + vb[2]*vBasis[element(0,2)];
		tmp[1]	=	vb[0]*vBasis[element(1,0)] + vb[1]*vBasis[element(1,1)] + vb[2]*vBasis[element(1,2)];
		tmp[2]	=	vb[0]*vBasis[element(2,0)] + vb[1]*vBasis[element(2,1)] + vb[2]*vBasis[element(2,2)];
		tmp[3]	=	vb[0]*vBasis[element(3,0)] + vb[1]*vBasis[element(3,1)] + vb[2]*vBasis[element(3,2)];

		dPdv[0]	=	tmp[0]*v0[0] + tmp[1]*v1[0] + tmp[2]*v2[0] + tmp[3]*v3[0];
		dPdv[1]	=	tmp[0]*v0[1] + tmp[1]*v1[1] + tmp[2]*v2[1] + tmp[3]*v3[1];
		dPdv[2]	=	tmp[0]*v0[2] + tmp[1]*v1[2] + tmp[2]*v2[2] + tmp[3]*v3[2];

		crossvv(dPdu,dPdv,P);
		crossvv(N,dPdu,dPdv);
		normalizevf(dPdu);
	}

	up	&=	~(PARAMETER_P | PARAMETER_NG | PARAMETER_DPDU | PARAMETER_DPDV | variables->parameters);
}


///////////////////////////////////////////////////////////////////////
// Class				:	CCubicCurve
// Method				:	dice
// Description			:	Dice the curve group into smaller ones
// Return Value			:	-
// Comments				:
// Date last edited		:	6/2/2003
void			CCubicCurve::splitToChildren(CShadingContext *rasterizer) {
	const float vmid = (vmin + vmax) * 0.5f;

	// Create vmin - vmid group
	rasterizer->drawObject(new CCubicCurve(attributes,xform,base,vmin,vmid,gvmin,gvmax));

	// Create vmid - vmax group
	rasterizer->drawObject(new CCubicCurve(attributes,xform,base,vmid,vmax,gvmin,gvmax));
}


















///////////////////////////////////////////////////////////////////////
// Class				:	CLinearCurve
// Method				:	CLinearCurve
// Description			:	Ctor
// Return Value			:	-
// Comments				:	
// Date last edited		:	6/2/2003
CLinearCurve::CLinearCurve(CAttributes *a,CXform *x,CBase *b,float vmi,float vma,float gvmi,float gvma) : CCurve(a,x,b,vmi,vma,gvmi,gvma) {

	// Compute the bounding box
	const CVertexData	*variables	=	base->variables;
	const int			vertexSize	=	variables->vertexSize;
	const float			*vertex		=	base->vertex;
	int					i;

	if (variables->moving)	i	=	4;
	else					i	=	2;

	initv(bmin,C_INFINITY,C_INFINITY,C_INFINITY);
	initv(bmax,-C_INFINITY,-C_INFINITY,-C_INFINITY);

	for (;i>0;i--,vertex+=vertexSize)	addBox(bmin,bmax,vertex);

	subvf(bmin,base->maxSize);
	addvf(bmax,base->maxSize);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CLinearCurve
// Method				:	~CLinearCurve
// Description			:	Dtor
// Return Value			:	-
// Comments				:
// Date last edited		:	6/2/2003
CLinearCurve::~CLinearCurve() {
}

///////////////////////////////////////////////////////////////////////
// Class				:	CLinearCurve
// Method				:	sample
// Description			:	Sample the curves
// Return Value			:	-
// Comments				:
// Date last edited		:	6/2/2003
void			CLinearCurve::sample(int start,int numVertices,float **varying,unsigned int &up) const {
	int				j,k;
	float			*intr,*intrStart;
	CVertexData		*variables			=	base->variables;
	const	int		vertexSize			=	variables->vertexSize;
	const	int		vs					=	(variables->moving ? vertexSize*2 : vertexSize);
	const	int		numSavedVertices	=	numVertices;
	const	float	*v0;
	const	float	*v1;

	intr	=	intrStart	=	(float *) alloca(numVertices*vertexSize*sizeof(float));

	if ((variables->moving == FALSE) || (up & PARAMETER_BEGIN_SAMPLE)) {
		v0					=	base->vertex;
		v1					=	v0 + vs;
	} else {
		v0					=	base->vertex + vertexSize;
		v1					=	v0 + vs;
	}

	const	float	*v = varying[VARIABLE_V];
	for (j=numVertices;j>0;j--) {
		const	float	cv	=	*v++;

		*intr++	=	v0[0]*(1-cv) + v1[0]*cv;
		*intr++	=	v0[1]*(1-cv) + v1[1]*cv;
		*intr++	=	v0[2]*(1-cv) + v1[2]*cv;

		for (k=3;k<vertexSize;k++) {
			*intr++			=	v0[k]*(1-cv) + v1[k]*cv;
		}
	}

	// Dispatch the variables
	variables->dispatch(intrStart,0,numSavedVertices,varying);

	// Compute the normal and derivatives
	float		*dPdv	=	varying[VARIABLE_DPDV] + start*3;
	float		*dPdu	=	varying[VARIABLE_DPDU] + start*3;
	const float	*P		=	varying[VARIABLE_P] + start*3;
	float		*N		=	varying[VARIABLE_NG] + start*3;

	for (j=numVertices;j>0;j--,P+=3,dPdu+=3,dPdv+=3,N+=3) {
		subvv(dPdv,v1,v0);
		crossvv(dPdu,dPdv,P);
		crossvv(N,dPdu,dPdv);
		normalizevf(dPdu);
	}

	up	&=	~(PARAMETER_P | PARAMETER_NG | PARAMETER_DPDU | PARAMETER_DPDV | variables->parameters);
}


///////////////////////////////////////////////////////////////////////
// Class				:	CLinearCurve
// Method				:	split
// Description			:	Dice the curve group into smaller ones
// Return Value			:	-
// Comments				:
// Date last edited		:	6/2/2003
void			CLinearCurve::splitToChildren(CShadingContext *rasterizer) {
	const float		vmid = (vmin + vmax) * 0.5f;

	// Create vmin - vmid group
	rasterizer->drawObject(new CLinearCurve(attributes,xform,base,vmin,vmid,gvmin,gvmax));

	// Create vmid - vmax group
	rasterizer->drawObject(new CLinearCurve(attributes,xform,base,vmid,vmax,gvmin,gvmax));
}














///////////////////////////////////////////////////////////////////////
// Function				:	CCurveMesh
// Description			:	CCurveMesh
// Return Value			:	Ctor
// Comments				:	-
// Date last edited		:	6/10/2003
CCurveMesh::CCurveMesh(CAttributes *a,CXform *x,CPl *c,int d,int nv,int nc,int *nve,int w) : CObject(a,x) {
	int			i;
	const float	*P;
	float		*vertex;

	stats.numGprims++;
	stats.gprimMemory		+=	sizeof(CCurveMesh) + sizeof(int)*nc;

	// Attach to the PL
	pl				=	c;

	// Save the data
	numVertices		=	nv;
	numCurves		=	nc;
	degree			=	d;
	nverts			=	new int[numCurves]; memcpy(nverts,nve,sizeof(int)*numCurves);
	wrap			=	w;

	// Extract the maximum width without touching the PL
	sizeVariable	=	NULL;
	maxSize			=	0;
	for (vertex=pl->data0,i=0;i<pl->numParameters;i++) {
		const CVariable	*cVar	=	pl->parameters[i].variable;

		if ((cVar->entry == VARIABLE_WIDTH) || (cVar->entry == VARIABLE_CONSTANTWIDTH)) {
			const	int	np	=	pl->parameters[i].numItems;

			assert(cVar->numFloats == 1);

			sizeVariable		=	cVar;

			for (i=0;i<np;i++) {
				maxSize			=	max(maxSize,vertex[i]);
			}

			if (pl->data1 != NULL) {
				vertex	=	pl->data1 + (vertex - pl->data0);

				for (i=0;i<np;i++) {
					maxSize			=	max(maxSize,vertex[i]);
				}
			}

			break;
		}

		vertex +=	pl->parameters[i].numItems*cVar->numFloats;
	}

	// Compute the bound
	initv(bmin,C_INFINITY,C_INFINITY,C_INFINITY);
	initv(bmax,-C_INFINITY,-C_INFINITY,-C_INFINITY);

	if (degree == 1) {
		vector	tmp;

		for (P=pl->data0,i=numVertices;i>0;i--,P+=3) {
			mulmp(tmp,xform->from,P);
			addBox(bmin,bmax,tmp);
		}

		if (pl->data1 != NULL) {
			for (P=pl->data1,i=numVertices;i>0;i--,P+=3) {
				mulmp(tmp,xform->from,P);
				addBox(bmin,bmax,tmp);
			}
		}
	} else {
		int				k			=	0;
		int				cVertex		=	0;
		matrix			geometryMatrix;

		assert(degree == 3);

		// Convert to Bezier Matrix
		// FIXME: The order of the multiplication may be wrong !!!
		mulmm(geometryMatrix,attributes->vBasis,invBezier);

		for (i=0;i<numCurves;i++) {
			const	int	ncsegs	=	(wrap == FALSE ? (nverts[i] - 4) / attributes->vStep + 1 : nverts[i] / attributes->vStep);
			int			j;

			for (j=0;j<ncsegs;j++,k++) {
				float	*v0		=	pl->data0 + (cVertex+(j*attributes->vStep + 0) % nverts[i])*3;
				float	*v1		=	pl->data0 + (cVertex+(j*attributes->vStep + 1) % nverts[i])*3;
				float	*v2		=	pl->data0 + (cVertex+(j*attributes->vStep + 2) % nverts[i])*3;
				float	*v3		=	pl->data0 + (cVertex+(j*attributes->vStep + 3) % nverts[i])*3;

				makeCubicBound(bmin,bmax,v0,v1,v2,v3,geometryMatrix,xform);

				if (pl->data1 != NULL) {
					v0			=	pl->data1 + (cVertex+(j*attributes->vStep + 0) % nverts[i])*3;
					v1			=	pl->data1 + (cVertex+(j*attributes->vStep + 1) % nverts[i])*3;
					v2			=	pl->data1 + (cVertex+(j*attributes->vStep + 2) % nverts[i])*3;
					v3			=	pl->data1 + (cVertex+(j*attributes->vStep + 3) % nverts[i])*3;

					makeCubicBound(bmin,bmax,v0,v1,v2,v3,geometryMatrix,xform);
				}
			}

			cVertex				+=	nverts[i];
		}
	}

	// Expand the bounding box by the width of the curves
	if (sizeVariable == NULL)	maxSize	=	1;
	maxSize		*=	0.5f*powf(fabsf(determinantm(xform->from)), 1.0f / 3.0f);
	addvf(bmax,maxSize);
	subvf(bmin,maxSize);

	// Make it a bound
	makeBound(bmin,bmax);
}

///////////////////////////////////////////////////////////////////////
// Function				:	CCurveMesh
// Description			:	~CCurveMesh
// Return Value			:	Dtor
// Comments				:	-
// Date last edited		:	6/10/2003
CCurveMesh::~CCurveMesh() {
	stats.numGprims--;
	stats.gprimMemory	-=	sizeof(CCurveMesh) + sizeof(int)*numCurves;

	delete pl;
	delete [] nverts;
}


///////////////////////////////////////////////////////////////////////
// Function				:	CCurveMesh
// Description			:	instantiate
// Return Value			:	Clone the object
// Comments				:	-
// Date last edited		:	6/10/2003
void	CCurveMesh::instantiate(CAttributes *a,CXform *x,CRendererContext *c) const {
	CXform	*nx	=	new CXform(x);

	nx->concat(xform);	// Concetenate the local xform

	if (a == NULL)	a	=	attributes;

	c->addObject(new CCurveMesh(a,nx,pl->clone(a),degree,numVertices,numCurves,nverts,wrap));
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCurveMesh
// Method				:	dice
// Description			:	Dice the primitive
// Return Value			:	-
// Comments				:
// Date last edited		:	5/28/2003
void	CCurveMesh::dice(CShadingContext *rasterizer) {

	if (children == NULL)	create(rasterizer);

	CObject::dice(rasterizer);
}


///////////////////////////////////////////////////////////////////////
// Class				:	CCurveMesh
// Method				:	dice
// Description			:	Dice the primitive
// Return Value			:	-
// Comments				:
// Date last edited		:	5/28/2003
void	CCurveMesh::create(CShadingContext *context) {

	osLock(CRenderer::hierarchyMutex);
	if (children != NULL) {
		osUnlock(CRenderer::hierarchyMutex);
		return;
	}

	int					i;
	CVertexData			*variables;
	int					vertexSize;
	float				*vertex;
	CObject				*allChildren;

	memBegin(context->threadMemory);

	// Extract the vertices
	vertex	=	NULL;
	pl->transform(xform);													// Transform the core
	pl->collect(vertexSize,vertex,CONTAINER_VERTEX,context->threadMemory);	// Obtain the vertex data

	// Multiply the curve width by the expansion in the coordinate system
	{
		const float expansion	=	powf(fabsf(determinantm(xform->from)), 1.0f / 3.0f);
		float		*vertex;
		for (vertex=pl->data0,i=0;i<pl->numParameters;i++) {
			const CVariable	*cVar	=	pl->parameters[i].variable;

			if (cVar == sizeVariable) {
				const	int	np	=	pl->parameters[i].numItems;

				for (i=0;i<np;i++) vertex[i] *=	expansion;

				if (pl->data1 != NULL) {
					vertex	=	pl->data1 + (vertex - pl->data0);

					for (i=0;i<np;i++)	vertex[i] *= expansion;
				}

				break;
			}

			vertex+=pl->parameters[i].numItems*cVar->numFloats;
		}
	}

	// Allocate the variables
	variables		=	pl->vertexData();
	allChildren		=	NULL;

	// Instanciate
	if (degree == 3) {
		float			*baseVertex;
		int				t			=	0;
		int				k			=	0;
		int				cVertex		=	0;

		for (baseVertex=vertex,i=0;i<numCurves;i++) {
			const	int	ncsegs	=	(wrap == FALSE ? (nverts[i] - 4) / attributes->vStep + 1 : nverts[i] / attributes->vStep);
			int			j;
			const	int	nvars	=	ncsegs + 1 - wrap;

			for (j=0;j<ncsegs;j++,k++) {
				float			*v0		=	baseVertex + (cVertex+(j*attributes->vStep + 0) % nverts[i])*vertexSize;
				float			*v1		=	baseVertex + (cVertex+(j*attributes->vStep + 1) % nverts[i])*vertexSize;
				float			*v2		=	baseVertex + (cVertex+(j*attributes->vStep + 2) % nverts[i])*vertexSize;
				float			*v3		=	baseVertex + (cVertex+(j*attributes->vStep + 3) % nverts[i])*vertexSize;
				CParameter		*parameters;
				CCurve			*cCurve;
				CCurve::CBase	*base	=	new CCurve::CBase;
				const float		vmin	=	j / (float) ncsegs;
				const float		vmax	=	(j+1) / (float) ncsegs;

				parameters			=	pl->uniform(i,NULL);
				parameters			=	pl->varying(t+j,t+(j+1)%nvars,parameters);

				variables->attach();
				base->maxSize		=	maxSize;
				base->variables		=	variables;
				base->sizeVariable	=	sizeVariable;
				base->parameters	=	parameters;
				base->vertex		=	new float[vertexSize*4];
				memcpy(base->vertex + 0*vertexSize,v0,vertexSize*sizeof(float));
				memcpy(base->vertex + 1*vertexSize,v1,vertexSize*sizeof(float));
				memcpy(base->vertex + 2*vertexSize,v2,vertexSize*sizeof(float));
				memcpy(base->vertex + 3*vertexSize,v3,vertexSize*sizeof(float));

				cCurve				=	new CCubicCurve(attributes,xform,base,0,1,vmin,vmax);
				cCurve->sibling		=	allChildren;
				allChildren			=	cCurve;	
			}

			cVertex					+=	nverts[i];
			t						+=	nvars;
		}
	} if (degree == 1) {
		float			*baseVertex;
		int				k			=	0;
		int				t			=	0;
		int				cVertex		=	0;

		for (baseVertex=vertex,i=0;i<numCurves;i++) {
			const	int	ncsegs	=	(wrap == FALSE ? nverts[i] - 1: nverts[i]);
			int			j;
			const	int	nvars	=	nverts[i];

			for (j=0;j<ncsegs;j++,k++) {
				float			*v0		=	baseVertex + (cVertex+(j + 0) % nverts[i])*vertexSize;
				float			*v1		=	baseVertex + (cVertex+(j + 1) % nverts[i])*vertexSize;
				CParameter		*parameters;
				CCurve			*cCurve;
				CCurve::CBase	*base	=	new CCurve::CBase;
				const float		vmin	=	j / (float) ncsegs;
				const float		vmax	=	(j+1) / (float) ncsegs;
				
				parameters			=	pl->uniform(i,NULL);
				parameters			=	pl->varying(t+j,t+(j+1)%nvars,parameters);

				variables->attach();
				base->maxSize		=	maxSize;
				base->variables		=	variables;
				base->sizeVariable	=	sizeVariable;
				base->refCount		=	0;
				base->parameters	=	parameters;
				base->vertex		=	new float[vertexSize*2];
				memcpy(base->vertex + 0*vertexSize,v0,vertexSize*sizeof(float));
				memcpy(base->vertex + 1*vertexSize,v1,vertexSize*sizeof(float));

				cCurve				=	new CLinearCurve(attributes,xform,base,0,1,vmin,vmax);
				cCurve->sibling		=	allChildren;
				allChildren			=	cCurve;
			}

			cVertex					+=	nverts[i];
			t						+=	nvars;
		}
	}

	memEnd(context->threadMemory);

	children	=	allChildren;

	// If raytraced, attach to the children
	if (raytraced()) {
		CObject	*cObject;
		for (cObject=allChildren;cObject!=NULL;cObject=cObject->sibling)	cObject->attach();
		cluster(context);
	}

	osUnlock(CRenderer::hierarchyMutex);
}












