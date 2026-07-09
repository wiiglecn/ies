#include "StdAfx.h"
#include "Utils.h"
#include "arxHeaders.h"
CUtils::CUtils(void)
{
}

CUtils::~CUtils(void)
{
}
void CUtils::FocusAcadDrawing()
{
    // Try to get the active document's viewport window
    CWnd* pDwgView = acedGetAcadDwgView();
    if (pDwgView && pDwgView->GetSafeHwnd())
    {
        ::SetFocus(pDwgView->GetSafeHwnd());
    }
    else 
    {
        // Fallback: If dwg view is not available, try the main frame
        if (acedGetAcadFrame())
            ::SetFocus(acedGetAcadFrame()->m_hWnd);
    }
}
bool CUtils::IsCircleExisting(const AcGePoint3d& center)
{
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) return false;

    AcDbBlockTable* pBlockTable = NULL;
    AcDbBlockTableRecord* pModelSpace = NULL;

    // Open Model Space for read
    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk)
        return false;
    
    if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForRead) != Acad::eOk)
    {
        pBlockTable->close();
        return false;
    }
    pBlockTable->close();

    bool found = false;
    AcDbBlockTableRecordIterator* pIter = NULL;
    if (pModelSpace->newIterator(pIter) == Acad::eOk)
    {
        for (pIter->start(); !pIter->done(); pIter->step())
        {
            AcDbEntity* pEnt = NULL;
            if (pIter->getEntity(pEnt, AcDb::kForRead) == Acad::eOk)
            {
                // Check if it's a circle
                AcDbCircle* pCircle = AcDbCircle::cast(pEnt);
                if (pCircle)
                {
                    // Check radius and center
                    // Using a small tolerance for floating point comparison
                    const double tol = 1e-4; 
                    if (pCircle->center().distanceTo(center) < tol)
                    {
                        found = true;
                        pCircle->close();
                        break; // Found one, no need to check further
                    }
                }
                pEnt->close();
            }
        }
        delete pIter;
    }
    pModelSpace->close();

    return found;
}