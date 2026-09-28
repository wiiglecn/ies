// LineHolePlacer.h
#pragma once

#include "dbmain.h"
#include "acdb.h"
#include "adslib.h"
#include "rxregsvc.h"
#include "BaseMarker.h"
class CLineHolePlacer : public CBaseMarker
{
public:
    CLineHolePlacer();
    ~CLineHolePlacer();

    // Main execution method
    Acad::ErrorStatus Execute();

private:
    // Helper to get a point from user
    Acad::ErrorStatus GetUserPoint(const TCHAR* prompt, AcGePoint3d& pt);
    
    // Helper to get an integer from user
    Acad::ErrorStatus GetUserInt(const TCHAR* prompt, int& val);
    
    // Helper to create a circle at a specific location
    Acad::ErrorStatus CreateHole(const AcGePoint3d& center, double radius = 0.5);
};