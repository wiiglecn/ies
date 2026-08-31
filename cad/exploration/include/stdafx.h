#ifdef _CAD2005
#include "StdAfx2005.h"
#else
#include "StdAfx2007.h"
#endif

// ºÊ»› VC7/æ… Platform SDK£∫winnt.h ÷–√ª”– ARRAYSIZE ∫Í
#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif