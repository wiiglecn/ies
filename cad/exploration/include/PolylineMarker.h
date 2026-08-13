#pragma once

#include "arxHeaders.h"
#include "BaseMarker.h"
class CPolylineMarker : public CBaseMarker
{
public:
    CPolylineMarker();
    ~CPolylineMarker();

    // Main entry point: Prompts user to select objects and processes them
    void ExecuteSelectionAndMark();
    
private:
    // Helper to draw a red circle at a specific point in Model Space
    void DrawRedCircle(const AcGePoint3d& center);

    // Helper to process a Polyline and draw circles at vertices
    void ProcessPolylineVertices(AcDbPolyline* pPline, 
        const AcGeMatrix3d& xform = AcGeMatrix3d::kIdentity);
	
};