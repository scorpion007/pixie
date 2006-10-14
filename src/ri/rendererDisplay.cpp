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
//  File				:	rendererDisplay.cpp
//  Classes				:	CRenderer
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#include <string.h>
#include <math.h>

#include "renderer.h"
#include "rendererContext.h"
#include "error.h"
#include "memory.h"
#include "stats.h"
#include "options.h"
#include "remoteChannel.h"




static COptions::CDisplay	*currentDisplay	=	NULL;

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
				return	CRenderer::colorQuantizer;
			} else {
				return	currentDisplay->quantizer;
			}
		}
	} else if (strcmp(name,"dither") == 0) {
		if ((numItems == 1) && (type == FLOAT_PARAMETER)) {
			if (currentDisplay->quantizer[0] == -1) {
				return	CRenderer::colorQuantizer + 4;
			} else {
				return	currentDisplay->quantizer + 4;
			}
		}
	} else if (strcmp(name,"near") == 0) {
		if ((numItems == 1) && (type == FLOAT_PARAMETER))		return	&CRenderer::clipMin;
	} else if (strcmp(name,"far") == 0) {
		if ((numItems == 1) && (type == FLOAT_PARAMETER))		return	&CRenderer::clipMax;
	} else if (strcmp(name,"Nl") == 0) {
		if ((numItems == 16) && (type == FLOAT_PARAMETER))		return	&CRenderer::fromWorld;
	} else if (strcmp(name,"NP") == 0) {
		if ((numItems == 16) && (type == FLOAT_PARAMETER))		return	&CRenderer::worldToNDC;
	} else if (strcmp(name,"gamma") == 0) {
		if ((numItems == 1) && (type == FLOAT_PARAMETER))		return	&CRenderer::gamma;
	} else if (strcmp(name,"gain") == 0) {
		if ((numItems == 1) && (type == FLOAT_PARAMETER))		return	&CRenderer::gain;
	} else if (strcmp(name,"Software") == 0) {
		if ((numItems == 1) && (type == STRING_PARAMETER))		return	(void *) "Pixie";
	}

	return	NULL;
}






///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	beginDisplays
// Description			:	Initiate the displays
// Return Value			:	-
// Comments				:
// Date last edited		:	8/26/2001
void	CRenderer::beginDisplays() {

	// Clear the data first
	numDisplays			=	0;
	numActiveDisplays	=	0;
	datas				=	NULL;

	deepShadowFile		=	NULL;
	deepShadowIndex		=	NULL;
	deepShadowIndexStart=	0;
	
	sampleOrder			=	NULL;
	sampleDefaults		=	NULL;

	// Initiate the displays
	if (!(hiderFlags & HIDER_NODISPLAY))
		computeDisplayData();
	else {
		numSamples			=	0;
		numExtraSamples		=	0;
	}

	// If we have a client, do not create a display
	if (netClient != INVALID_SOCKET) {
		numActiveDisplays	=	1;
	}

	// Start a TSM channel if needed
	if ((netClient != INVALID_SOCKET) && (flags & OPTIONS_FLAGS_DEEP_SHADOW_RENDERING)) {
		requestRemoteChannel(new CRemoteTSMChannel(deepShadowFileName,deepShadowFile,deepShadowIndex,xBuckets,yBuckets));
	}
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	endDisplays
// Description			:	Terminate the displays
// Return Value			:	-
// Comments				:
// Date last edited		:	8/26/2001
void	CRenderer::endDisplays() {
	int	i;

	// Finish the out images
	for (i=0;i<numDisplays;i++) {
		if (datas[i].module != NULL) {
			datas[i].finish(datas[i].handle);
			if (strcmp(datas[i].display->outDevice,RI_SHADOW) == 0) {
				CRenderer::context->RiMakeShadowV(datas[i].displayName,datas[i].displayName,0,NULL,NULL);
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
// Class				:	CRenderer
// Method				:	dispatch
// Description			:	Dispatch a rendered window to the out devices
// Return Value			:	-
// Comments				:
// Date last edited		:	8/26/2001
void	CRenderer::dispatch(int left,int top,int width,int height,float *pixels) {
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
// Class				:	CRenderer
// Method				:	clear
// Description			:	Send a clear window to the out devices
// Return Value			:	-
// Comments				:
// Date last edited		:	8/26/2001
void	CRenderer::clear(int left,int top,int width,int height) {
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
// Class				:	CRenderer
// Method				:	commit
// Description			:	Send a window of samples
// Return Value			:	-
// Comments				:
// Date last edited		:	8/26/2001
void	CRenderer::commit(int left,int top,int xpixels,int ypixels,float *pixels) {
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
		if (renderTop > 0)		clear(0,0,xres,renderTop);
	}

	if (left == 0) {
		if (renderLeft > 0)		clear(0,top+renderTop,renderLeft,ypixels);
	}

	if ((left+xpixels) == xPixels) {
		if (renderRight < xres)	clear(renderRight,top+renderTop,xres-renderRight,ypixels);
	}

	if (((top+ypixels) == yPixels) && ((left+xpixels) == xPixels)) {
		if (renderBottom < yres)	clear(0,renderBottom,xres,yres-renderBottom);
	}


	dispatch(left+renderLeft,top+renderTop,xpixels,ypixels,pixels);
}



///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	getDisplayName
// Description			:	Create the display name
// Return Value			:
// Comments				:
// Date last edited		:	7/4/2001
void	CRenderer::getDisplayName(char *out,const char *in,const char *displayType) {
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
				sprintf(cOut,widthString,(int) frame);
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
// Class				:	CRenderer
// Method				:	computeDisplayData
// Description			:	Compute the display data
// Return Value			:
// Comments				:
// Date last edited		:	7/4/2001
void	CRenderer::computeDisplayData() {
	COptions::CDisplay	*cDisplay;
	CDisplayChannel		*oChannel;
	char				displayName[OS_MAX_PATH_LENGTH];
	char				deviceFile[OS_MAX_PATH_LENGTH];
	char				*sampleDefinition,*sampleName,*nextComma,*tmp;
	int					i,j,k,s,t,isNewChannel;

	// mark all channels as unallocated
	resetDisplayChannelUsage();
	
	for (i=0,cDisplay=displays;cDisplay!=NULL;i++,cDisplay=cDisplay->next);

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

	for (cDisplay=displays;cDisplay!=NULL;cDisplay=cDisplay->next) {
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
			oChannel = retrieveDisplayChannel(sampleName);
			if (oChannel != NULL) {
				// it's a predefined / already seen channel
			
				if (oChannel->variable != NULL) {
					// variable is NULL only for RGBAZ channels
					if (hiderFlags & HIDER_RGBAZ_ONLY) {
						error(CODE_BADTOKEN,"Hider %s can not handle display channels\n",hider);
						dspError = TRUE;
						break;
					}	
					
					// Make sure the variable is global
					if (oChannel->outType == -1) {
						makeGlobalVariable(oChannel->variable);
						oChannel->outType = oChannel->variable->entry;
					}
				}
			} else {
				// it's an old-style AOV
				
				if (hiderFlags & HIDER_RGBAZ_ONLY) {
					error(CODE_BADTOKEN,"Hider %s can not handle arbitrary output variables\n",hider);
					dspError = TRUE;
					break;
				} else {				
					CVariable	*cVar		=	retrieveVariable(sampleName);
					
					// if it's an inline declaration but it's not defined yet, declare it
					if (cVar == NULL) {
						cVar = declareVariable(NULL,sampleName);
					}
	
					if (cVar != NULL) {
						// Make sure the variable is global
						if (cVar->storage != STORAGE_GLOBAL) {
							makeGlobalVariable(cVar);
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
					oChannel = declareDisplayChannel(cVar);
					
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
			header.xres		=	xres;
			header.yres		=	yres;
			header.xTiles	=	xBuckets;
			header.yTiles	=	yBuckets;
			header.tileSize	=	bucketWidth;
			for (header.tileShift=1;(1 << header.tileShift) < bucketWidth;header.tileShift++);
			movmm(header.toNDC,worldToNDC);

			// The sanity check
			if ((1 << header.tileShift) != bucketWidth) {
				error(CODE_LIMIT,"Bucket width must be a power of 2 for tsm (%d).\n",bucketWidth);
			} else {
				if (bucketWidth != bucketHeight) {
					error(CODE_LIMIT,"Bucket width and height must be same for tsm (%d,%d).\n",bucketWidth,bucketHeight);
				} else {
					if (strcmp(hider,"stochastic") != 0) {
						error(CODE_LIMIT,"Hider must be stochastic / hidden for tsm.\n");
					} else {
						if (deepShadowFile != NULL) {
							error(CODE_LIMIT,"There can only be one tsm output.\n");
						} else {
							if (netClient != INVALID_SOCKET) {
								char tempTsmName[OS_MAX_PATH_LENGTH];
								
								if (!osFileExists(temporaryPath)) {
									osCreateDir(temporaryPath);
								}
								
								// need read and write
								osTempname(temporaryPath,"rndr",tempTsmName);
								deepShadowFile		=	ropen(tempTsmName,"w+b",fileTransparencyShadow);
								
								// register temporary for deletion
								registerFrameTemporary(tempTsmName,TRUE);
							} else {
								deepShadowFile		=	ropen(displayName,"wb",fileTransparencyShadow);
							}
	
							if (deepShadowFile != NULL) {
								numActiveDisplays++;
								flags						|=	OPTIONS_FLAGS_DEEP_SHADOW_RENDERING;
	
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
	
										tsmThreshold	=	val[0];
									}
								}
							}
						}
					}
				}
			}
		} else if (netClient == INVALID_SOCKET) {
			if (locateFileEx(deviceFile,outDevice,osModuleExtension,displayPath)) {
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
						datas[numDisplays].handle	=	datas[numDisplays].start(displayName,xres,yres,datas[numDisplays].numSamples,cDisplay->outSamples,findParameter);
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



