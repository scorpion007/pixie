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
//  File				:	frame.cpp
//  Classes				:	CFrame
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#include <string.h>
#include <math.h>

#include "frame.h"
#include "stats.h"
#include "memory.h"
#include "error.h"
#include "object.h"
#include "brickmap.h"
#include "photonMap.h"
#include "pointCloud.h"
#include "texture3d.h"
#include "irradiance.h"
#include "stats.h"
#include "random.h"
#include "points.h"
#include "radiance.h"
#include "remoteChannel.h"
#include "shading.h"



static COptions::CDisplay	*currentDisplay	=	NULL;




/////////////////////////////////////////////////////////////////////
// Static members of the CFrame
/////////////////////////////////////////////////////////////////////
COptions				CFrame::options;
CVariable				**CFrame::globalVariables;
int						CFrame::numGlobalVariables;
int						CFrame::maxGlobalVariables;
CDictionary<const char *,CFileResource *>	*CFrame::loadedFiles;
CDictionary<const char *,CRemoteChannel *>	*CFrame::declaredRemoteChannels;
CArray<CRemoteChannel *>					*CFrame::remoteChannels;
CArray<CAttributes *>						*CFrame::dirtyAttributes;
CArray<CProgrammableShaderInstance *>		*CFrame::dirtyInstances;
unsigned int			CFrame::raytracingFlags;
CHierarchy				*CFrame::hierarchy;
CArray<CTriangle *>		*CFrame::triangles;
CArray<CSurface *>		*CFrame::raytraced;
CArray<CTracable *>		*CFrame::tracables;
matrix					CFrame::fromWorld,CFrame::toWorld;
matrix					CFrame::fromNDC,CFrame::toNDC;
matrix					CFrame::fromRaster,CFrame::toRaster;
matrix					CFrame::fromScreen,CFrame::toScreen;
matrix					CFrame::worldToNDC;
unsigned int			CFrame::hiderFlags;
int						CFrame::numSamples;
int						CFrame::numExtraSamples;
int						CFrame::xPixels,CFrame::yPixels;
unsigned int			CFrame::additionalParameters;
float					CFrame::pixelLeft,CFrame::pixelRight,CFrame::pixelTop,CFrame::pixelBottom;
float					CFrame::dydPixel,CFrame::dxdPixel;
float					CFrame::dPixeldx,CFrame::dPixeldy;
int						CFrame::renderLeft,CFrame::renderRight,CFrame::renderTop,CFrame::renderBottom;
int						CFrame::xBuckets,CFrame::yBuckets;
int						CFrame::metaXBuckets,CFrame::metaYBuckets;
float					CFrame::aperture;
float					CFrame::imagePlane;
float					CFrame::invImagePlane;
float					CFrame::cocFactorPixels;
float					CFrame::cocFactorSamples;
float					CFrame::cocFactorScreen;
float					CFrame::invFocaldistance;
float					CFrame::lengthA,CFrame::lengthB;
float					CFrame::marginXcoverage,CFrame::marginYcoverage;
float					CFrame::marginX,CFrame::marginY;
float					CFrame::marginalX,CFrame::marginalY;
float					CFrame::leftX,CFrame::leftZ,CFrame::leftD;
float					CFrame::rightX,CFrame::rightZ,CFrame::rightD;
float					CFrame::topY,CFrame::topZ,CFrame::topD;
float					CFrame::bottomY,CFrame::bottomZ,CFrame::bottomD;
int						CFrame::numActiveDisplays;
int						CFrame::currentXBucket;
int						CFrame::currentYBucket;
SOCKET					CFrame::netClient;
int						*CFrame::jobAssignment;
FILE					*CFrame::deepShadowFile;
int						*CFrame::deepShadowIndex;
int						CFrame::deepShadowIndexStart;
char					*CFrame::deepShadowFileName;














///////////////////////////////////////////////////////////////////////
// Function				:	findParameter
// Description			:	This function can be used by the display server to probe for parameters
// Return Value			:	-
// Comments				:
// Date last edited		:	7/4/2001
void	*findParameter(const char *name,ParameterType type,int numItems) {
	if (currentDisplay != NULL) {
		int	i;

		for (i=0;i<currentDisplay->numParameters;i++) {
			if (strcmp(name,currentDisplay->parameters[i].name) == 0) {
				if (numItems == currentDisplay->parameters[i].numItems) {
					if (type == currentDisplay->parameters[i].type) {
						return	currentDisplay->parameters[i].data;
					}
				}
			}
		}
	}

	if (strcmp(name,"quantize") == 0) {
		if ((numItems == 4) && (type == FLOAT_PARAMETER))	{
			if (currentDisplay->quantizer[0] == -1) {
				return	CFrame::options.colorQuantizer;
			} else {
				return	currentDisplay->quantizer;
			}
		}
	} else if (strcmp(name,"dither") == 0) {
		if ((numItems == 1) && (type == FLOAT_PARAMETER)) {
			if (currentDisplay->quantizer[0] == -1) {
				return	CFrame::options.colorQuantizer + 4;
			} else {
				return	currentDisplay->quantizer + 4;
			}
		}
	} else if (strcmp(name,"near") == 0) {
		if ((numItems == 1) && (type == FLOAT_PARAMETER))		return	&CFrame::options.clipMin;
	} else if (strcmp(name,"far") == 0) {
		if ((numItems == 1) && (type == FLOAT_PARAMETER))		return	&CFrame::options.clipMax;
	} else if (strcmp(name,"Nl") == 0) {
		if ((numItems == 16) && (type == FLOAT_PARAMETER))		return	&CFrame::fromWorld;
	} else if (strcmp(name,"NP") == 0) {
		if ((numItems == 16) && (type == FLOAT_PARAMETER))		return	&CFrame::worldToNDC;
	} else if (strcmp(name,"gamma") == 0) {
		if ((numItems == 1) && (type == FLOAT_PARAMETER))		return	&CFrame::options.gamma;
	} else if (strcmp(name,"gain") == 0) {
		if ((numItems == 1) && (type == FLOAT_PARAMETER))		return	&CFrame::options.gain;
	} else if (strcmp(name,"Software") == 0) {
		if ((numItems == 1) && (type == STRING_PARAMETER))		return	(void *) "Pixie";
	}

	return	NULL;
}



///////////////////////////////////////////////////////////////////////
// Class				:	CFrame
// Method				:	beginFrame
// Description			:	Begin a frame / compute misc data
// Return Value			:	-
// Comments				:
// Date last edited		:	10/9/2006
void		CFrame::beginFrame(const COptions *o,const CXform *x,SOCKET s,unsigned int hf) {

	options					=	*o;
	movmm(fromWorld,x->from);
	movmm(toWorld,x->to);

	hiderFlags				=	hf;
	netClient				=	s;

	assert(options.pixelXsamples > 0);
	assert(options.pixelYsamples > 0);

	// Compute some stuff
	if (options.flags & OPTIONS_FLAGS_CUSTOM_FRAMEAR) {
		const float	ar	=	options.xres*options.pixelAR / (float) options.yres;

		// Update the resolution as necessary
		if (options.frameAR > ar) {
			options.yres	=	(int) (options.xres*options.pixelAR / options.frameAR);
		} else {
			options.xres	=	(int) (options.frameAR * options.yres / options.pixelAR);
		}
	} else {
		options.frameAR = options.xres*options.pixelAR / (float) options.yres;
	}


	if (options.flags & OPTIONS_FLAGS_CUSTOM_SCREENWINDOW) {
		// The user explicitly entered the screen window, so we don't have to make sure it matches the frame aspect ratio
	} else {
		if (options.frameAR > (float) 1.0) {
			options.screenTop			=	(float) 1.0;
			options.screenBottom		=	(float) -1.0;
			options.screenLeft			=	-options.frameAR;
			options.screenRight			=	options.frameAR;
		} else {
			options.screenTop			=	1/options.frameAR;
			options.screenBottom		=	-1/options.frameAR;
			options.screenLeft			=	(float) -1.0;
			options.screenRight			=	(float) 1.0;
		}
	}

	imagePlane		=	1;
	if (options.projection == OPTIONS_PROJECTION_PERSPECTIVE) {
		imagePlane	=	(float) (1/tan(radians(options.fov/(float) 2)));
	} else {
		imagePlane	=	1;
	}

	invImagePlane	=	1/imagePlane;

	assert(options.cropLeft < options.cropRight);
	assert(options.cropTop < options.cropBottom);

	// Rendering window in pixels
	renderLeft			=	(int) ceil(options.xres*options.cropLeft);
	renderRight			=	(int) ceil(options.xres*options.cropRight);
	renderTop			=	(int) ceil(options.yres*options.cropTop);
	renderBottom		=	(int) ceil(options.yres*options.cropBottom);

	assert(renderRight > renderLeft);
	assert(renderBottom > renderTop);

	// The resolution of the actual render window
	xPixels				=	renderRight		-	renderLeft;
	yPixels				=	renderBottom	-	renderTop;

	assert(xPixels >= 0);
	assert(yPixels >= 0);

	dxdPixel			=	(options.screenRight	- options.screenLeft) / (float) (options.xres);
	dydPixel			=	(options.screenBottom	- options.screenTop) / (float) (options.yres);
	dPixeldx			=	1	/	dxdPixel;
	dPixeldy			=	1	/	dydPixel;
	pixelLeft			=	(float) (options.screenLeft	+ renderLeft*dxdPixel);
	pixelTop			=	(float) (options.screenTop	+ renderTop*dydPixel);
	pixelRight			=	pixelLeft	+ dxdPixel*xPixels;
	pixelBottom			=	pixelTop	+ dydPixel*yPixels;

	xBuckets			=	(int) ceil(xPixels / (float) options.bucketWidth);
	yBuckets			=	(int) ceil(yPixels / (float) options.bucketHeight);

	metaXBuckets		=	(int) ceil(xBuckets / (float) options.netXBuckets);
	metaYBuckets		=	(int) ceil(yBuckets / (float) options.netYBuckets);

	jobAssignment		=	NULL;

	aperture			=	options.focallength / (2*options.fstop);
	if ((aperture <= C_EPSILON) || (options.projection == OPTIONS_PROJECTION_ORTHOGRAPHIC)) {
		aperture			=	0;
		cocFactorScreen		=	0;
		cocFactorSamples	=	0;
		cocFactorPixels		=	0;
		invFocaldistance	=	0;
	} else {
		cocFactorScreen		=	(float) (imagePlane*aperture*options.focaldistance /  (options.focaldistance + aperture));
		cocFactorSamples	=	cocFactorScreen*sqrtf(dPixeldx*dPixeldx*options.pixelXsamples*options.pixelXsamples + dPixeldy*dPixeldy*options.pixelYsamples*options.pixelYsamples);
		cocFactorPixels		=	cocFactorScreen*sqrtf(dPixeldx*dPixeldx + dPixeldy*dPixeldy);
		invFocaldistance	=	1 / options.focaldistance;
	}

	if (options.projection == OPTIONS_PROJECTION_ORTHOGRAPHIC) {
		lengthA			=	0;
		lengthB			=	sqrtf(dxdPixel*dxdPixel + dydPixel*dydPixel);
	} else {
		lengthA			=	sqrtf(dxdPixel*dxdPixel + dydPixel*dydPixel) / imagePlane;
		lengthB			=	0;
	}

	if (aperture				!= 0)					options.flags	|=	OPTIONS_FLAGS_FOCALBLUR;
	if (options.shutterClose	!= options.shutterOpen)	options.flags	|=	OPTIONS_FLAGS_MOTIONBLUR;

	// Compute the matrices related to the camera transformation
	if (options.projection == OPTIONS_PROJECTION_PERSPECTIVE) {
		toNDC[element(0,0)]		=	imagePlane / (options.screenRight - options.screenLeft);
		toNDC[element(0,1)]		=	0;
		toNDC[element(0,2)]		=	-options.screenLeft / (options.screenRight - options.screenLeft);
		toNDC[element(0,3)]		=	0;

		toNDC[element(1,0)]		=	0;
		toNDC[element(1,1)]		=	imagePlane / (options.screenBottom - options.screenTop);
		toNDC[element(1,2)]		=	-options.screenTop / (options.screenBottom - options.screenTop);
		toNDC[element(1,3)]		=	0;

		toNDC[element(2,0)]		=	0;
		toNDC[element(2,1)]		=	0;
		toNDC[element(2,2)]		=	1;
		toNDC[element(2,3)]		=	0;

		toNDC[element(3,0)]		=	0;
		toNDC[element(3,1)]		=	0;
		toNDC[element(3,2)]		=	1;
		toNDC[element(3,3)]		=	0;
	} else {
		toNDC[element(0,0)]		=	1 / (options.screenRight - options.screenLeft);
		toNDC[element(0,1)]		=	0;
		toNDC[element(0,2)]		=	0;
		toNDC[element(0,3)]		=	-options.screenLeft / (options.screenRight - options.screenLeft);

		toNDC[element(1,0)]		=	0;
		toNDC[element(1,1)]		=	1 / (options.screenBottom - options.screenTop);
		toNDC[element(1,2)]		=	0;
		toNDC[element(1,3)]		=	-options.screenTop / (options.screenBottom - options.screenTop);

		toNDC[element(2,0)]		=	0;
		toNDC[element(2,1)]		=	0;
		toNDC[element(2,2)]		=	1;
		toNDC[element(2,3)]		=	0;

		toNDC[element(3,0)]		=	0;
		toNDC[element(3,1)]		=	0;
		toNDC[element(3,2)]		=	0;
		toNDC[element(3,3)]		=	1;
	}

	// The inverse fromNDC is the same for both perspective and orthographic projections
	fromNDC[element(0,0)]	=	(options.screenRight - options.screenLeft);
	fromNDC[element(0,1)]	=	0;
	fromNDC[element(0,2)]	=	0;
	fromNDC[element(0,3)]	=	options.screenLeft;

	fromNDC[element(1,0)]	=	0;
	fromNDC[element(1,1)]	=	(options.screenBottom - options.screenTop);
	fromNDC[element(1,2)]	=	0;
	fromNDC[element(1,3)]	=	options.screenTop;

	fromNDC[element(1,0)]	=	0;
	fromNDC[element(1,1)]	=	0;
	fromNDC[element(1,2)]	=	0;
	fromNDC[element(1,3)]	=	imagePlane;

	fromNDC[element(3,0)]	=	0;
	fromNDC[element(3,1)]	=	0;
	fromNDC[element(3,2)]	=	0;
	fromNDC[element(3,3)]	=	1;


	// Compute the fromRaster / toRaster
	matrix	mtmp;

	identitym(mtmp);
	mtmp[element(0,0)]		=	(float) options.xres;
	mtmp[element(1,1)]		=	(float) options.yres;
	mulmm(toRaster,mtmp,toNDC);

	identitym(mtmp);
	mtmp[element(0,0)]		=	1 / (float) options.xres;
	mtmp[element(1,1)]		=	1 / (float) options.yres;
	mulmm(fromRaster,fromNDC,mtmp);

	// Compute the world to NDC transform required by the shadow maps
	mulmm(worldToNDC,toNDC,fromWorld);

	const float	minX		=	min(pixelLeft,pixelRight);	// The extend of the rendering window on the image
	const float	maxX		=	max(pixelLeft,pixelRight);	// plane
	const float	minY		=	min(pixelTop,pixelBottom);
	const float	maxY		=	max(pixelTop,pixelBottom);

	// Compute the equations of the clipping planes
	// The visible points are:
	// Px*leftX		+ Pz*leftZ		+ leftD		>=	0	&&
	// Px*rightX	+ Pz*rightZ		+ rightD	>=	0	&&
	// Py*topY		+ Pz*topZ		+ topD		>=	0	&&
	// Py*bottomY	+ Pz*bottomZ	+ bottomD	>=	0	&&
	// Pz >= clipMin									&&
	// Pz <= clipMax
	if (options.projection == OPTIONS_PROJECTION_PERSPECTIVE) {
		leftX			=	imagePlane;
		leftZ			=	-minX;
		leftD			=	0;
		rightX			=	-imagePlane;
		rightZ			=	maxX;
		rightD			=	0;
		topY			=	imagePlane;
		topZ			=	-minY;
		topD			=	0;
		bottomY			=	-imagePlane;
		bottomZ			=	maxY;
		bottomD			=	0;
	} else {
		leftX			=	1;
		leftZ			=	0;
		leftD			=	-minX;
		rightX			=	-1;
		rightZ			=	0;
		rightD			=	maxX;

		topY			=	-1;
		topZ			=	0;
		topD			=	maxY;
		bottomY			=	1;
		bottomZ			=	0;
		bottomD			=	-minY;
	}

	if (options.displays == NULL) {
		options.displays				=	new CDisplay;
		options.displays->next			=	NULL;
		options.displays->outDevice		=	strdup(RI_FILE);
		options.displays->outName		=	strdup("ri.tif");
		options.displays->outSamples	=	strdup(RI_RGBA);
	}

	marginalX			=	options.pixelFilterWidth / 2;
	marginalY			=	options.pixelFilterHeight / 2;
	marginX				=	(float) floor(marginalX);
	marginY				=	(float) floor(marginalY);
	marginXcoverage		=	max(marginalX - marginX,0);
	marginYcoverage		=	max(marginalY -	marginY,0);

	currentXBucket		=	0;
	currentYBucket		=	0;

	numDisplays			=	0;
	numActiveDisplays	=	0;
	datas				=	NULL;

	deepShadowFile		=	NULL;
	deepShadowIndex		=	NULL;
	deepShadowIndexStart=	0;
	
	sampleOrder			=	NULL;
	sampleDefaults		=	NULL;

	if (netClient != INVALID_SOCKET) {
		numActiveDisplays		=	1;
	}

	if (!(hiderFlags & HIDER_NODISPLAY))
		computeDisplayData();
	else {
		numSamples		=	0;
		numExtraSamples	=	0;
	}
}

///////////////////////////////////////////////////////////////////////
// Class				:	CFrame
// Method				:	endFrame
// Description			:	Finish the frame
// Return Value			:	-
// Comments				:
// Date last edited		:	10/9/2006
void		CFrame::endFrame() {
	int			i;

	// Delete the job queue
	if (jobAssignment	!= NULL)	delete [] jobAssignment;

	// Finish the out images
	for (i=0;i<numDisplays;i++) {
		if (datas[i].module != NULL) {
			datas[i].finish(datas[i].handle);
			if (strcmp(datas[i].display->outDevice,RI_SHADOW) == 0) {
				currentRenderer->RiMakeShadowV(datas[i].displayName,datas[i].displayName,0,NULL,NULL);
			}
		}
		if (datas[i].displayName != NULL) free(datas[i].displayName);
		delete[] datas[i].channels;
	}

	if (datas != NULL)			delete[] datas;
	if (sampleOrder != NULL)	delete[] sampleOrder;
	if (sampleDefaults != NULL)	delete[] sampleDefaults;
	
	if (deepShadowFile != NULL) {
		fseek(deepShadowFile,deepShadowIndexStart,SEEK_SET);
		fwrite(deepShadowIndex,sizeof(int),xBuckets*yBuckets*2,deepShadowFile);	// Override the deep shadow map index
		fclose(deepShadowFile);
	}

	if (deepShadowIndex != NULL) {
		delete [] deepShadowIndex;
		free(deepShadowFileName);
	}
}





///////////////////////////////////////////////////////////////////////
// Class				:	CFrame
// Method				:	advanceBucket
// Description			:	Advance the bucket for network parallel rendering
// Return Value			:	TRUE if we're still rendering, FALSE otherwise
// Comments				:
// Date last edited		:	7/4/2001
int				CFrame::advanceBucket(int index,int &x,int &y,int &nx,int &ny) {

	nx = xBuckets;
	ny = yBuckets;
	
// Advance bucket indices
#define	advance(__x,__y)									\
		__x++;												\
		if (__x == xBuckets) {								\
			__x	=	0;										\
			__y++;											\
			if (__y == yBuckets)	{						\
				return FALSE;								\
			}												\
		}

// Find the server index assigned to this job
#define	bucket(__x,__y)		jobAssignment[__y*xBuckets + __x]

	if (jobAssignment == FALSE) {
		int	i;

		jobAssignment	=	new int[xBuckets*yBuckets];

		// Create the job assignment
		for (i=0;i<xBuckets*yBuckets;i++)	jobAssignment[i]	=	-1;

	}

	// Are we just starting ?
	if ((x == -1) || (y == -1)) {
		x	=	0;			// Begin from the start
		y	=	0;
	} else {
		advance(x,y);		// Advance the bucket by one
	}
	
	// Scan forward from (cx,cy) to find the first bucket to render
	while(TRUE) {

		// Has the bucket been assigned before ?
		if (bucket(x,y) == -1) {
			int	left	=	(x / options.netXBuckets)*options.netXBuckets;
			int	right	=	min((left + options.netXBuckets),xBuckets);
			int	top		=	(y / options.netYBuckets)*options.netYBuckets;
			int	bottom	=	min((top + options.netYBuckets),yBuckets);
			int	i,j;

			// The bucket is not assigned ...
			// Assign the meta block to this processor
			for (i=left;i<right;i++) {
				for (j=top;j<bottom;j++) {
					bucket(i,j)	=	index;
				}
			}

			assert(bucket(x,y) == index);

			// We found the job !!!
			return TRUE;
		} else if (bucket(x,y) != index) {

			// This bucket has been pre-allocated to another server, skip over
			advance(x,y);
		} else {

			// This bucket has been pre-allocated to us, proceed
			return TRUE;
		}
	}
}

///////////////////////////////////////////////////////////////////////
// Class				:	CFrame
// Method				:	dispatch
// Description			:	Dispatch a rendered window to the out devices
// Return Value			:	-
// Comments				:
// Date last edited		:	8/26/2001
void	CFrame::dispatch(int left,int top,int width,int height,float *pixels) {
	float	*dest,*dispatch;
	int		i,j,k,l;
	int		srcStep,dstStep,disp;

	// Send the pixels to the output servers
	for (i=0;i<numDisplays;i++) {
		if (datas[i].module != NULL) {
			int				imageSamples	=	datas[i].numSamples;
			
			memBegin();
	
			dispatch	= (float *) ralloc(width*height*imageSamples*sizeof(float));
			
			for (j=0,disp=0;j<datas[i].numChannels;j++){
				const float		*tmp			=	&pixels[datas[i].channels[j].sampleStart];
				const int		channelSamples	=	datas[i].channels[j].numSamples;
				
				dest		=	dispatch + disp;
				srcStep		=	numSamples - channelSamples;
				dstStep		=	imageSamples - channelSamples;
				disp		+=	channelSamples;
	
				for (k=width*height;k>0;k--) {
					for (l=channelSamples;l>0;l--) {
						*dest++	=	*tmp++;				// GSHTODO: quantize here, not in driver
					}
					tmp		+=	srcStep;
					dest	+=	dstStep;
				}
			}

			if (datas[i].data(datas[i].handle,left,top,width,height,dispatch) == FALSE) {
				datas[i].handle	=	NULL;
				numActiveDisplays--;
				if (numActiveDisplays == 0)	hiderFlags	|=	HIDER_BREAK;
				osUnloadModule(datas[i].module);
				datas[i].module	=	NULL;
			}

			memEnd();
		}
	}
}

///////////////////////////////////////////////////////////////////////
// Class				:	CFrame
// Method				:	clear
// Description			:	Send a clear window to the out devices
// Return Value			:	-
// Comments				:
// Date last edited		:	8/26/2001
void	CFrame::clear(int left,int top,int width,int height) {
	memBegin();

	float	* pixels	=	(float *) ralloc(width*height*numSamples*sizeof(float));
	int		i;

	for (i=0;i<width*height*numSamples;i++) {
		pixels[i]	=	0;
	}

	dispatch(left,top,width,height,pixels);

	memEnd();
}

///////////////////////////////////////////////////////////////////////
// Class				:	CFrame
// Method				:	commit
// Description			:	Send a window of samples
// Return Value			:	-
// Comments				:
// Date last edited		:	8/26/2001
void	CFrame::commit(int left,int top,int xpixels,int ypixels,float *pixels) {
	if (netClient != INVALID_SOCKET) {
		// We are rendering for a client, so just send the result to the waiting client
		T32	header[5];
		T32	a;

		header[0].integer	=	NET_READY;
		rcSend(netClient,(char *) header,	1*sizeof(T32));

		header[0].integer	=	left;
		header[1].integer	=	top;
		header[2].integer	=	xpixels;
		header[3].integer	=	ypixels;
		header[4].integer	=	xpixels*ypixels*numSamples;

		rcSend(netClient,(char *) header,	5*sizeof(T32));
		rcRecv(netClient,(char *) &a,		1*sizeof(T32));
		rcSend(netClient,(char *) pixels,xpixels*ypixels*numSamples*sizeof(T32));
		return;
	}

	if ((top == 0) && (left == 0)) {
		if (renderTop > 0)		clear(0,0,options.xres,renderTop);
	}

	if (left == 0) {
		if (renderLeft > 0)		clear(0,top+renderTop,renderLeft,ypixels);
	}

	if ((left+xpixels) == xPixels) {
		if (renderRight < options.xres)	clear(renderRight,top+renderTop,options.xres-renderRight,ypixels);
	}

	if (((top+ypixels) == yPixels) && ((left+xpixels) == xPixels)) {
		if (renderBottom < options.yres)	clear(0,renderBottom,options.xres,options.yres-renderBottom);
	}


	dispatch(left+renderLeft,top+renderTop,xpixels,ypixels,pixels);
}



///////////////////////////////////////////////////////////////////////
// Class				:	CFrame
// Method				:	getDisplayName
// Description			:	Create the display name
// Return Value			:
// Comments				:
// Date last edited		:	7/4/2001
void	CFrame::getDisplayName(char *out,const char *in,const char *displayType) {
	char		*cOut	=	out;
	const char	*cIn	=	in;

	while(*cIn != '\0') {
		if (*cIn == '#') {
			int		width		=	0;
			char	widthString[256];

			cIn++;

			while((*cIn >= '0') && (*cIn <= '9')) {
				widthString[width++]	=	*cIn++;
			}

			if (width > 0) {
				widthString[width]	=	'\0';
				sscanf(widthString,"%d",&width);
				sprintf(widthString,"%%0%dd",width);
			} else {
				sprintf(widthString,"%%d");
			}

			switch(*cIn++) {
			case 'f':
				sprintf(cOut,widthString,(int) options.frame);
				while(*cOut != '\0')	cOut++;
				break;
			case 's':
				sprintf(cOut,widthString,stats.sequenceNumber);
				while(*cOut != '\0')	cOut++;
				break;
			case 'n':
				sprintf(cOut,widthString,stats.runningSequenceNumber);
				while(*cOut != '\0')	cOut++;
				break;
			case 'h':
				char	hostName[1024];
				gethostname(hostName,1024);
				sprintf(cOut,hostName);
				while(*cOut != '\0')	cOut++;
				break;
			case 'd':
				strcpy(cOut,displayType);
				while(*cOut != '\0')	cOut++;
				break;
			case 'p':
				sprintf(cOut,"0");
				while(*cOut != '\0')	cOut++;
				break;
			case 'P':
				sprintf(cOut,"0");
				while(*cOut != '\0')	cOut++;
				break;
			case '#':
				sprintf(cOut,"#");
				while(*cOut != '\0')	cOut++;
				break;
			default:
				error(CODE_BADTOKEN,"Unknown display stub %c\n",*cIn);
				break;
			}
		} else {
			*cOut++	=	*cIn++;
		}
	}

	*cOut	=	'\0';
}









///////////////////////////////////////////////////////////////////////
// Class				:	CFrame
// Method				:	inFrustrum
// Description			:	Check if the given box is inside the viewing frustrum
// Return Value			:	-
// Comments				:
// Date last edited		:	5/10/2002
int			CFrame::inFrustrum(const float *bmin,const float *bmax) {
	vector	corners[8];
	int		i;

	initv(corners[0],bmin[COMP_X],bmin[COMP_Y],bmin[COMP_Z]);
	initv(corners[1],bmin[COMP_X],bmax[COMP_Y],bmin[COMP_Z]);
	initv(corners[2],bmin[COMP_X],bmax[COMP_Y],bmax[COMP_Z]);
	initv(corners[3],bmin[COMP_X],bmin[COMP_Y],bmax[COMP_Z]);
	initv(corners[4],bmax[COMP_X],bmin[COMP_Y],bmin[COMP_Z]);
	initv(corners[5],bmax[COMP_X],bmax[COMP_Y],bmin[COMP_Z]);
	initv(corners[6],bmax[COMP_X],bmax[COMP_Y],bmax[COMP_Z]);
	initv(corners[7],bmax[COMP_X],bmin[COMP_Y],bmax[COMP_Z]);

	// Check against the left bounding plane
	for (i=0;i<8;i++) {
		const float	*corner	=	corners[i];

		if ((corner[COMP_X]*leftX + corner[COMP_Z]*leftZ + leftD) > 0) {
			break;
		}
	}

	if (i == 8)	return FALSE;


	// Check against the right bounding plane
	for (i=0;i<8;i++) {
		const float	*corner	=	corners[i];

		if ((corner[COMP_X]*rightX + corner[COMP_Z]*rightZ + rightD) > 0) {
			break;
		}
	}

	if (i == 8)	return	FALSE;

	// Check against the top bounding plane
	for (i=0;i<8;i++) {
		const float	*corner	=	corners[i];

		if ((corner[COMP_Y]*topY + corner[COMP_Z]*topZ + topD) > 0) {
			break;
		}
	}

	if (i == 8)	return	FALSE;


	// Check against the bottom bounding plane
	for (i=0;i<8;i++) {
		const float	*corner	=	corners[i];

		if ((corner[COMP_Y]*bottomY + corner[COMP_Z]*bottomZ + bottomD) > 0) {
			break;
		}
	}

	if (i == 8)	return	FALSE;

	return	TRUE;
}


///////////////////////////////////////////////////////////////////////
// Class				:	CFrame
// Method				:	inFrustrum
// Description			:	Check if the given box is inside the viewing frustrum
// Return Value			:	-
// Comments				:
// Date last edited		:	5/10/2002
int			CFrame::inFrustrum(const float *P) {

	if ((P[COMP_X]*leftX + P[COMP_Z]*leftZ + leftD) < 0) {
		return FALSE;
	}

	if ((P[COMP_X]*rightX + P[COMP_Z]*rightZ + rightD) < 0) {
		return FALSE;
	}

	if ((P[COMP_Y]*topY + P[COMP_Z]*topZ + topD) < 0) {
		return FALSE;
	}

	if ((P[COMP_Y]*bottomY + P[COMP_Z]*bottomZ + bottomD) < 0) {
		return FALSE;
	}

	return	TRUE;
}




///////////////////////////////////////////////////////////////////////
// Class				:	CFrame
// Method				:	clipCode
// Description			:	Compute the clipping codes for a point
// Return Value			:	-
// Comments				:
// Date last edited		:	5/10/2002
unsigned int			CFrame::clipCode(const float *P) {
	unsigned int	code	=	0;

	if ((P[COMP_X]*leftX + P[COMP_Z]*leftZ + leftD) < 0) {
		code	|=	CLIP_LEFT;
	}

	if ((P[COMP_X]*rightX + P[COMP_Z]*rightZ + rightD) < 0) {
		code	|=	CLIP_RIGHT;
	}

	if ((P[COMP_Y]*topY + P[COMP_Z]*topZ + topD) < 0) {
		code	|=	CLIP_TOP;
	}

	if ((P[COMP_Y]*bottomY + P[COMP_Z]*bottomZ + bottomD) < 0) {
		code	|=	CLIP_BOTTOM;
	}

	if (P[COMP_Z] < options.clipMin)	code	|=	CLIP_NEAR;

	if (P[COMP_Z] > options.clipMax)	code	|=	CLIP_FAR;

	return	code;
}








///////////////////////////////////////////////////////////////////////
// Class				:	CFrame
// Method				:	render
// Description			:	Add an object into the scene
// Return Value			:
// Comments				:
// Date last edited		:	10/9/2006
void			CFrame::render(CShadingContext *context,CObject *cObject,const float *bmin,const float *bmax) {
	CAttributes	*cAttributes	=	cObject->attributes;

	// Assign the photon map is necessary
	if (cAttributes->globalMapName != NULL) {
		cAttributes->globalMap	=	getPhotonMap(cAttributes->globalMapName);
		cAttributes->globalMap->attach();
	}

	if (cAttributes->causticMapName != NULL) {
		cAttributes->causticMap	=	getPhotonMap(cAttributes->causticMapName);
		cAttributes->causticMap->attach();
	}

	if ((cAttributes->globalMap != NULL) || (cAttributes->causticMap != NULL)) {
		if (dirtyAttributes == NULL) dirtyAttributes	=	new CArray<CAttributes *>;

		cAttributes->attach();
		dirtyAttributes->push(cAttributes);
	}

	// Update the world bounding box
	addBox(worldBmin,worldBmax,bmin);
	addBox(worldBmin,worldBmax,bmax);

	// Tesselate the object if applicable
	if (cObject->attributes->flags & raytracingFlags) {
		cObject->tesselate(context);
	}
}

///////////////////////////////////////////////////////////////////////
// Class				:	CFrame
// Method				:	remove
// Description			:	Remove a delayed object
// Return Value			:
// Comments				:
// Date last edited		:	10/9/2006
void			CFrame::remove(CTracable *cObject) {
	if (hierarchy != NULL) {
		vector	bmin,bmax;

		// Bound the object
		cObject->bound(bmin,bmax);

		hierarchy->remove(cObject,bmin,bmax);
	}
}









///////////////////////////////////////////////////////////////////////
// Class				:	CFrame
// Method				:	computeDisplayData
// Description			:	Compute the display data
// Return Value			:
// Comments				:
// Date last edited		:	7/4/2001
void	CFrame::computeDisplayData() {
	CDisplay		*cDisplay;
	CDisplayChannel *oChannel;
	char			displayName[OS_MAX_PATH_LENGTH];
	char			deviceFile[OS_MAX_PATH_LENGTH];
	char			*sampleDefinition,*sampleName,*nextComma,*tmp;
	int				i,j,k,s,t,isNewChannel;

	// mark all channels as unallocated
	currentRenderer->resetDisplayChannelUsage();
	
	for (i=0,cDisplay=options.displays;cDisplay!=NULL;i++,cDisplay=cDisplay->next);

	datas					=	new CDisplayData[i];
	j						=	0;
	numSamples				=	5;	// rgbaz
	numExtraSamples			=	0;
	numDisplays				=	0;
	numExtraChannels		=	0;
	additionalParameters	=	0;
	numActiveDisplays		=	0;
	deepShadowFile			=	NULL;
	deepShadowIndex			=	NULL;

	for (cDisplay=options.displays;cDisplay!=NULL;cDisplay=cDisplay->next) {
		datas[numDisplays].display		=	cDisplay;
		datas[numDisplays].displayName	=	NULL;
		datas[numDisplays].numChannels	=	0;
		datas[numDisplays].channels		=	NULL;
		datas[numDisplays].numSamples	=	0;
		
		int dspError					=	FALSE;
		int dspNumExtraChannels			=	0;
		int dspNumSamples				=	0;
		int dspNumExtraSamples			=	0;
		
		// work out how many channels to expect (at least one)
		int dspNumChannels = 1;
		sampleDefinition = cDisplay->outSamples;
		while ((sampleDefinition = strchr(sampleDefinition,',')) != NULL) {
			sampleDefinition++;
			dspNumChannels++;
		}
		datas[numDisplays].channels = new CDisplayChannel[dspNumChannels];
		
		// parse the channels / sample types
		sampleDefinition = strdup(cDisplay->outSamples); // duplicate to tokenize
		nextComma = sampleName = sampleDefinition;
		dspNumChannels = 0;
		do {
			// parse to next comma, remove spaces
			nextComma = strchr(sampleName,',');
			if (nextComma != NULL) {
				for (tmp=nextComma-1;isspace(*tmp)&&(tmp>sampleName);tmp--) *tmp = '\0';
				*nextComma++ = '\0';
				while (isspace(*nextComma)) nextComma++;
			}
			while (isspace(*sampleName)) sampleName++;
			
			// is the sample name a channel we know about?
			oChannel = currentRenderer->retrieveDisplayChannel(sampleName);
			if (oChannel != NULL) {
				// it's a predefined / already seen channel
			
				if (oChannel->variable != NULL) {
					// variable is NULL only for RGBAZ channels
					if (hiderFlags & HIDER_RGBAZ_ONLY) {
						error(CODE_BADTOKEN,"Hider %s can not handle display channels\n",options.hider);
						dspError = TRUE;
						break;
					}	
					
					// Make sure the variable is global
					if (oChannel->outType == -1) {
						currentRenderer->makeGlobalVariable(oChannel->variable);
						oChannel->outType = oChannel->variable->entry;
					}
				}
			} else {
				// it's an old-style AOV
				
				if (hiderFlags & HIDER_RGBAZ_ONLY) {
					error(CODE_BADTOKEN,"Hider %s can not handle arbitrary output variables\n",options.hider);
					dspError = TRUE;
					break;
				} else {				
					CVariable	*cVar		=	currentRenderer->retrieveVariable(sampleName);
					
					// if it's an inline declaration but it's not defined yet, declare it
					if (cVar == NULL) {
						cVar = currentRenderer->declareVariable(NULL,sampleName);
					}
	
					if (cVar != NULL) {
						// Make sure the variable is global
						if (cVar->storage != STORAGE_GLOBAL) {
							currentRenderer->makeGlobalVariable(cVar);
						} else if (cVar->storage == STORAGE_PARAMETER || cVar->storage == STORAGE_MUTABLEPARAMETER) {
							error(CODE_BADTOKEN,"Unable to find output variable or display channel \"%s\" (seems to be a parameter?)\n",sampleName);
							dspError = TRUE;
							break;
						}
					} else {
						error(CODE_BADTOKEN,"Unable to find output variable or display channel \"%s\"\n",sampleName);
						dspError = TRUE;
						break;
					}
										
					// now create a channel for the variable
					oChannel = currentRenderer->declareDisplayChannel(cVar);
					
					if (oChannel == NULL) {
						error(CODE_BADTOKEN,"variable \"%s\" clashes with a display channel\n",cVar->name);
						dspError = TRUE;
						break;
					}
				}
			}
			
			// record channel if it's new
			isNewChannel = FALSE;
			if (oChannel->sampleStart == -1) {
				// sampleStart is -1 only for channels not yet allocated
				oChannel->sampleStart		=	numSamples + dspNumSamples;
				dspNumSamples				+=	oChannel->numSamples;
				dspNumExtraSamples			+=	oChannel->numSamples;
				additionalParameters		|=	oChannel->variable->usageMarker;
				dspNumExtraChannels++;
				isNewChannel = TRUE;
			}
			memcpy(datas[numDisplays].channels + dspNumChannels,oChannel,sizeof(CDisplayChannel));
			if (oChannel->fill) {
				// ensure a deep copy
				datas[numDisplays].channels[dspNumChannels].fill = (float*) malloc(sizeof(float)*oChannel->numSamples);
				for(s=0;s<datas[numDisplays].channels[dspNumChannels].numSamples;s++)
					datas[numDisplays].channels[dspNumChannels].fill[s] = oChannel->fill[s];
			}
			if (isNewChannel == FALSE) {
				// mark this channel as a duplicate
				datas[numDisplays].channels[dspNumChannels].variable = NULL;
			}
			
			datas[numDisplays].numSamples		+= oChannel->numSamples;
			dspNumChannels++;
			
			sampleName = nextComma;
		} while((sampleName != NULL) && (*sampleName != '\0'));
		
		free(sampleDefinition);
		
		if (dspError) {
			error(CODE_BADTOKEN,"display \"%s\" disabled\n",cDisplay->outName);
			delete [] datas[numDisplays].channels;
			continue;
		}
		
		// Sum up if we successfully allocated display
		datas[numDisplays].numChannels	=	dspNumChannels;
		numSamples						+=	dspNumSamples;
		numExtraSamples					+=	dspNumExtraSamples;
		numExtraChannels				+=	dspNumExtraChannels;

		// finally deal with the display initialization
		getDisplayName(displayName,cDisplay->outName,cDisplay->outSamples);
		
		// save the computed display name
		datas[numDisplays].displayName = strdup(displayName);
		
		char * outDevice = cDisplay->outDevice;
		if (strcmp(outDevice,"shadow") == 0)	outDevice	= 	RI_FILE;
		if (strcmp(outDevice,"zfile") == 0)		outDevice	=	RI_FILE;
		if (strcmp(outDevice,"tiff") == 0)		outDevice	=	RI_FILE;
			
		if (strcmp(outDevice,"tsm") == 0) {
			int					j;
			CDeepShadowHeader	header;

			// The TSM is hardcoded
			datas[numDisplays].module	=	NULL;
			datas[numDisplays].handle	=	NULL;

			// Set up the file header
			header.xres		=	options.xres;
			header.yres		=	options.yres;
			header.xTiles	=	xBuckets;
			header.yTiles	=	yBuckets;
			header.tileSize	=	options.bucketWidth;
			for (header.tileShift=1;(1 << header.tileShift) < options.bucketWidth;header.tileShift++);
			movmm(header.toNDC,worldToNDC);

			// The sanity check
			if ((1 << header.tileShift) != options.bucketWidth) {
				error(CODE_LIMIT,"Bucket width must be a power of 2 for tsm (%d).\n",options.bucketWidth);
			} else {
				if (options.bucketWidth != options.bucketHeight) {
					error(CODE_LIMIT,"Bucket width and height must be same for tsm (%d,%d).\n",options.bucketWidth,options.bucketHeight);
				} else {
					if (strcmp(options.hider,"stochastic") != 0) {
						error(CODE_LIMIT,"Hider must be stochastic / hidden for tsm.\n");
					} else {
						if (deepShadowFile != NULL) {
							error(CODE_LIMIT,"There can only be one tsm output.\n");
						} else {
							if (netClient != INVALID_SOCKET) {
								char tempTsmName[OS_MAX_PATH_LENGTH];
								
								if (!osFileExists(options.temporaryPath)) {
									osCreateDir(options.temporaryPath);
								}
								
								// need read and write
								osTempname(options.temporaryPath,"rndr",tempTsmName);
								deepShadowFile		=	ropen(tempTsmName,"w+b",fileTransparencyShadow);
								
								// register temporary for deletion
								currentRenderer->registerFrameTemporary(tempTsmName,TRUE);
							} else {
								deepShadowFile		=	ropen(displayName,"wb",fileTransparencyShadow);
							}
	
							if (deepShadowFile != NULL) {
								numActiveDisplays++;
								options.flags						|=	OPTIONS_FLAGS_DEEP_SHADOW_RENDERING;
	
								deepShadowIndex				=	new int[xBuckets*yBuckets*2];
								deepShadowFileName			=	strdup(displayName);
								
								// Write the header
								fwrite(&header,sizeof(CDeepShadowHeader),1,deepShadowFile);
	
								// Save the index start
								deepShadowIndexStart	=	ftell(deepShadowFile);
	
								// Write the dummy index
								fwrite(deepShadowIndex,sizeof(int),xBuckets*yBuckets*2,deepShadowFile);
	
								// Parse the tsm parameters
								for (j=0;j<cDisplay->numParameters;j++) {
									if (strcmp(cDisplay->parameters[j].name,"threshold") == 0) {
										float	*val	=	(float *) cDisplay->parameters[j].data;
	
										options.tsmThreshold	=	val[0];
									}
								}
							}
						}
					}
				}
			}
		} else if (netClient == INVALID_SOCKET) {
			if (currentRenderer->locateFileEx(deviceFile,outDevice,osModuleExtension,options.displayPath)) {
				datas[numDisplays].module		=	osLoadModule(deviceFile);
				if (datas[numDisplays].module != NULL) {
					datas[numDisplays].start		=	(TDisplayStartFunction)		osResolve(datas[numDisplays].module,"displayStart");
					datas[numDisplays].data			=	(TDisplayDataFunction)		osResolve(datas[numDisplays].module,"displayData");
					datas[numDisplays].rawData		=	(TDisplayRawDataFunction)	osResolve(datas[numDisplays].module,"displayRawData");
					datas[numDisplays].finish		=	(TDisplayFinishFunction)	osResolve(datas[numDisplays].module,"displayFinish");

					if ((datas[numDisplays].start == NULL) || (datas[numDisplays].data == NULL) || (datas[numDisplays].finish == NULL)) {
						error(CODE_SYSTEM,"The module %s has missing implementation\n",deviceFile);
						osUnloadModule(datas[numDisplays].module);
						datas[numDisplays].module	=	NULL;
					} else {
						currentDisplay				=	cDisplay;
						datas[numDisplays].handle	=	datas[numDisplays].start(displayName,options.xres,options.yres,datas[numDisplays].numSamples,cDisplay->outSamples,findParameter);
							//GSHTODO: above sample names are now quite incorrect
						if (datas[numDisplays].handle != NULL) {
							numActiveDisplays++;
						} else {
							osUnloadModule(datas[numDisplays].module);
							datas[numDisplays].module	=	NULL;
						}
					}
				} else {
					datas[numDisplays].module		=	NULL;
					error(CODE_SYSTEM,"Unable to open out device \"%s\" (error: %s)\n",cDisplay->outDevice,osModuleError());
				}
			} else {
				datas[numDisplays].module		=	NULL;
				error(CODE_SYSTEM,"Unable to find out device \"%s\"\n",cDisplay->outDevice);
			}
		} else {
			datas[numDisplays].module	=	NULL;
			datas[numDisplays].handle	=	NULL;
		}

		numDisplays++;
	}
	
	// copy the sample and sampled defaults order for fast access
	sampleOrder		=	new int[numExtraChannels*2];
	sampleDefaults	=	new float[numExtraSamples];
	for (i=0,k=0,t=0;i<numDisplays;i++) {
		for (j=0;j<datas[i].numChannels;j++) {
			// skip standard channels
			if (datas[i].channels[j].outType == -1) continue;
			// skip duplicate channels
			if (datas[i].channels[j].variable == NULL) continue;
			
			if (datas[i].channels[j].fill) {
				for(s=0;s<datas[i].channels[j].numSamples;s++)
					sampleDefaults[t+s] = datas[i].channels[j].fill[s];
			} else {
				for(s=0;s<datas[i].channels[j].numSamples;s++) sampleDefaults[t+s] = 0;
			}
			t += datas[i].channels[j].numSamples;
			
			sampleOrder[k++] = datas[i].channels[j].outType;
			sampleOrder[k++] = datas[i].channels[j].numSamples;
		}
	}
	assert(k == 2*numExtraChannels);

	if (numActiveDisplays == 0) hiderFlags	|=	HIDER_BREAK;
}
