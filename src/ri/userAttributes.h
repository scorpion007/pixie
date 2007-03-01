//////////////////////////////////////////////////////////////////////
//
//                             Pixie
//
// Copyright � 1999 - 2003, Okan Arikan
//
// Contact: okan@cs.utexas.edu
//
//	This library is free software; you can redistribute it and/or
//	modify it under the terms of the GNU Lesser General Public
//	License as published by the Free Software Foundation; either
//	version 2.1 of the License, or (at your option) any later version.
//
//	This library is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//	Lesser General Public License for more details.
//
//	You should have received a copy of the GNU Lesser General Public
//	License along with this library; if not, write to the Free Software
//	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
//
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
//
//  File				:	userAttributes.h
//  Classes				:	CUserAttribute
//  Description			:	Holds user attributes dictionary
//
////////////////////////////////////////////////////////////////////////

#include "variable.h"

#ifndef USER_ATTRIBUTES_H
#define USER_ATTRIBUTES_H


class CUserAttributeDictionary {	
	CVariable *attribs;

public:
	CUserAttributeDictionary() : attribs(NULL) { }
	
	~CUserAttributeDictionary() {
		CVariable *cAttr = attribs;
		while (cAttr != NULL)  {
			CVariable *tAttr = cAttr->next;
			if (cAttr->type == TYPE_STRING) {
				char **tmp = (char**) cAttr->defaultValue;
				for (int i=0;i<cAttr->numFloats;i++) free(tmp[i]);
				delete[] (char**) cAttr->defaultValue; 
			} else {
				delete[] (float*) cAttr->defaultValue;
			}
			delete cAttr;
			cAttr = tAttr;
		}
	}
	
	CUserAttributeDictionary &operator=(const CUserAttributeDictionary &rhs) {
		attribs = NULL;//FIXME
		
		CVariable *cAttr = rhs.attribs;
		CVariable *nAttr = NULL;
		
		while (cAttr != NULL)  {
			CVariable *tAttr = new CVariable;
			*tAttr = *cAttr;
			// allocate and copy default value
			if (tAttr->type == TYPE_STRING) {
				tAttr->defaultValue	= (float*) new char *[cAttr->numFloats];
				char **src			= (char**) cAttr->defaultValue;
				for (int i = 0; i < tAttr->numFloats;	i++) {
					((char**) tAttr->defaultValue)[i] = strdup(src[i]);
				}
			} else {
				tAttr->defaultValue = new float[cAttr->numFloats];
				memcpy(tAttr->defaultValue,cAttr->defaultValue,sizeof(float)*cAttr->numFloats);
			}
			// maintain list
			if (nAttr == NULL) attribs = tAttr;
			tAttr->next = NULL;
			cAttr	=	cAttr->next;
			nAttr	=	tAttr;
		}
		
		return *this;
	}
	
	void insert(CVariable *var,void *value) {
		CVariable *cAttr	=	attribs;
		CVariable *pAttr	=	NULL;
		
		while (cAttr != NULL)  {
			int cmp = strcmp(var->name,cAttr->name);
			
			if (cmp > 0)	break;	// past the place it would be
			if (cmp == 0)	{		// exact match, remove this element
				if (pAttr != NULL)	pAttr->next	=	cAttr->next;
				else				attribs		=	cAttr->next;
				
				if (cAttr->type == TYPE_STRING) {
					char **tmp = (char**) cAttr->defaultValue;
					for (int i=0;i<cAttr->numFloats;i++) free(tmp[i]);
					delete[] (char**) cAttr->defaultValue; 
				} else {
					delete[] (float*) cAttr->defaultValue;
					delete cAttr;
				}
				
				// unlink the dead attribute
				if (pAttr == NULL)	cAttr = NULL;
				else				cAttr = pAttr->next;
				break;
			}
			
			// Advance
			pAttr = cAttr;
			cAttr = cAttr->next;
		}
		
		// pAttr now points to the loation _before_ which to insert
		CVariable *nAttr = new CVariable;
		*nAttr = *var;
		
		if (pAttr != NULL) {
			// insert after pAttr
			nAttr->next	= 	cAttr;
			pAttr->next =	nAttr;
		} else {
			// insert at the front
			nAttr->next =	attribs;
			attribs		=	nAttr;
		}
		
		// store the value
		if (var->type == TYPE_STRING) {
			nAttr->defaultValue	= (float*) new char *[var->numFloats];
			char **src						= (char**) value;
			for (int i = 0; i < var->numFloats;	i++) {
				((char**) nAttr->defaultValue)[i] = strdup(src[i]);
			}
		} else {
			nAttr->defaultValue = new float[var->numFloats];
			memcpy(nAttr->defaultValue,value,sizeof(float)*var->numFloats);
		}
	}
	
	int lookup(const char *name, CVariable *&var) const {
		CVariable *cAttr	=	attribs;
		while (cAttr != NULL)  {
			int cmp = strcmp(name,cAttr->name);
			
			if (cmp > 0)	return FALSE;	// past the place it would be
			if (cmp == 0)	{				// exact match, return this element
				var = cAttr;
				return TRUE;
			}
			
			cAttr = cAttr->next;
		}
		return FALSE;
	}
};

#endif