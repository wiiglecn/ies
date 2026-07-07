// coding: gb18030
// LineHolePlacer.cpp
#include "StdAfx.h"
#include "LineHolePlacer.h"
#include "LineJig.h"
//#include "arxHeaders.h"
//#include "acadstrc.h"

CLineHolePlacer::CLineHolePlacer()
{
}

CLineHolePlacer::~CLineHolePlacer()
{
}

Acad::ErrorStatus CLineHolePlacer::Execute()
{
    //Acad::ErrorStatus es;
    AcGePoint3d ptStart, ptEnd;
    int nHoles = 0;

	CLineJig lineJig;
    Acad::ErrorStatus es = lineJig.startJig();
    
    if (es == Acad::eOk)
    {
        acutPrintf(_T("\n直线绘制完成。"));

    }
    else if (es == Acad::eCancel)
    {
        acutPrintf(_T("\n命令已取消。"));
		return Acad::eUserBreak;

    }
    else
    {
        acutPrintf(_T("\n发生错误。"));
		return Acad::eNoErrorHandler;
    }
	ptStart = lineJig.getStartPoint();
	ptEnd  = lineJig.getEndPoint();

    //// Step 1: Get Start Point
    //es = GetUserPoint(_T("Specify first point of line: \n"), ptStart);
    //if (es != Acad::eOk) return es;

    //// Step 1: Get End Point
    //es = GetUserPoint(_T("Specify second point of line: \n"), ptEnd);
    //if (es != Acad::eOk) return es;

    // Step 2: Get Number of Holes
    es = GetUserInt(_T("\nEnter number of exploration holes: "), nHoles);
    if (es != Acad::eOk) return es;

    if (nHoles <= 0)
    {
        acutPrintf(_T("\nError: Number of holes must be greater than 0.\n"));
        return Acad::eInvalidInput;
    }

    // Step 3: Calculate positions and draw circles
    AcGeVector3d vec = ptEnd - ptStart;
    double totalLength = vec.length();
    
    if (totalLength < 1e-6)
    {
        acutPrintf(_T("\nError: Points are too close.\n"));
        return Acad::eInvalidInput;
    }

    // Normalize vector
    vec.normalize();

    // Calculate step distance
    // If nHoles is 1, it goes in the middle? Or at start? 
    // Usually "average distance" implies distributing them along the segment.
    // Let's assume distribution from start to end inclusive or exclusive?
    // Common interpretation: Divide segment into (nHoles - 1) intervals if placing at ends too,
    // or nHoles + 1 intervals if spacing from ends.
    // Let's assume standard linear distribution: 
    // Step = TotalLength / (nHoles - 1) if nHoles > 1, else 0 (single point at start? or mid?)
    // Let's use: Step = TotalLength / (nHoles + 1) to keep them inside the segment with margins?
    // Or simply: Step = TotalLength / (nHoles - 1) and place first at start, last at end.
    
    double stepDist = 0.0;
    if (nHoles == 1)
    {
        stepDist = 0.0; // Just one hole, let's put it at the midpoint
        ptStart = ptStart + vec * (totalLength / 2.0);
    }
    else
    {
        stepDist = totalLength / (nHoles - 1);
    }

	AcApDocument* pDoc = acDocManager->curDocument();

	es = acDocManager->lockDocument(pDoc);
    if (es != Acad::eOk) {
        acutPrintf(_T("Failed to lock the document..."));
		return Acad::eInvalidOpenState;
    }

    for (int i = 0; i < nHoles; ++i)
    {
        AcGePoint3d ptHole;
        if (nHoles == 1)
        {
            ptHole = ptStart; // Already calculated as midpoint above
        }
        else
        {
            ptHole = ptStart + vec * (stepDist * i);
        }
        
        es = CreateHole(ptHole);
        if (es != Acad::eOk)
        {
            acutPrintf(_T("\nError creating hole %d.\n"), i + 1);
            return es;
        }
    }

    acutPrintf(_T("\nSuccessfully placed %d exploration holes.\n"), nHoles);

	acDocManager->unlockDocument(pDoc);

    return Acad::eOk;
}

Acad::ErrorStatus CLineHolePlacer::GetUserPoint(const TCHAR* prompt, AcGePoint3d& pt)
{
    ads_point adsPt;
    int rc = acedGetPoint(NULL, prompt, adsPt);
    if (rc != RTNORM)
    {
        return Acad::eUserBreak;
    }
    pt.set(adsPt[X], adsPt[Y], adsPt[Z]);
    return Acad::eOk;
}

Acad::ErrorStatus CLineHolePlacer::GetUserInt(const TCHAR* prompt, int& val)
{
    int rc = acedGetInt(prompt, &val);
    if (rc != RTNORM)
    {
        return Acad::eUserBreak;
    }
    return Acad::eOk;
}

Acad::ErrorStatus CLineHolePlacer::CreateHole(const AcGePoint3d& center, double radius)
{
    Acad::ErrorStatus es;
    
    // Create a new circle entity
    AcDbCircle* pCircle = new AcDbCircle();
    pCircle->setCenter(center);
    pCircle->setRadius(radius);
    pCircle->setNormal(AcGeVector3d::kZAxis); // Assume XY plane
	pCircle->setColorIndex(1); // Red color

    // Add to current space (Model Space)
    AcDbBlockTable* pBlockTable = NULL;
    es = acdbHostApplicationServices()->workingDatabase()->getBlockTable(pBlockTable, AcDb::kForRead);
    if (es != Acad::eOk)
    {
        delete pCircle;
        return es;
    }

    AcDbBlockTableRecord* pBlockRecord = NULL;
    es = pBlockTable->getAt(ACDB_MODEL_SPACE, pBlockRecord, AcDb::kForWrite);
    pBlockTable->close();
    if (es != Acad::eOk)
    {
        delete pCircle;
        return es;
    }

    AcDbObjectId objId;
    es = pBlockRecord->appendAcDbEntity(objId, pCircle);
    pBlockRecord->close();
    
    if (es != Acad::eOk)
    {
        delete pCircle;
        return es;
    }

    pCircle->close();
    return Acad::eOk;
}