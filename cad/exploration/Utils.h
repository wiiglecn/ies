#pragma once
#include "dbents.h"
#include "gedll.h"
#include "gepnt3d.h"

class CUtils
{
public:
	CUtils(void);
public:
	~CUtils(void);
	static bool IsCircleExisting(const AcGePoint3d& center);
	static void FocusAcadDrawing();
};
