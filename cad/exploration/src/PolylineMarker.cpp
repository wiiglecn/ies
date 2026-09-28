// coding: gb18030
#include "StdAfx.h"
#include "PolylineMarker.h"
#include "arxHeaders.h"
#include "Utils.h"

CPolylineMarker::CPolylineMarker():CBaseMarker()
{
}

CPolylineMarker::~CPolylineMarker()
{
}

void CPolylineMarker::DrawRedCircle(const AcGePoint3d& center)
{
    // 1. Check if circle already exists
	if (CUtils::IsCircleExisting(center))
    {
        // Optional: Debug print to verify skipping
        // acutPrintf(_T("\n[SKIP] DrawRedCircle: Circle already exists at (%.3f, %.3f, %.3f)"), center.x, center.y, center.z);
        return;
    }
    CUtils::acutPrintf(_T("[DEBUG] DrawRedCircle: Center(%.3f, %.3f, %.3f) \n"), center.x, center.y, center.z);

    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) 
    {
        CUtils::acutPrintf(_T("[ERROR] DrawRedCircle: Failed to get working database. \n"));
        return;
    }

    AcDbBlockTable* pBlockTable = NULL;
    AcDbBlockTableRecord* pModelSpace = NULL;

    // Open Model Space for write
    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk)
    {
        CUtils::acutPrintf(_T("[ERROR] DrawRedCircle: Failed to open Block Table. \n"));
        return;
    }
    
    if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForWrite) != Acad::eOk)
    {
        CUtils::acutPrintf(_T("[ERROR] DrawRedCircle: Failed to open Model Space for write. \n"));
        pBlockTable->close();
        return;
    }
    pBlockTable->close();

    // Create Circle
    AcDbCircle* pCircle = new AcDbCircle();
    pCircle->setCenter(center);
    pCircle->setRadius(this->m_Diameter / 2.0); // Adjust radius as needed
    pCircle->setColorIndex(1); // Red color

    // Add to Model Space
    AcDbObjectId circleId;
    if (pModelSpace->appendAcDbEntity(circleId, pCircle) == Acad::eOk)
    {
        CUtils::acutPrintf(_T("[INFO] DrawRedCircle: Circle created successfully. \n"));
        pCircle->close();
    }
    else
    {
        CUtils::acutPrintf(_T("[ERROR] DrawRedCircle: Failed to append circle to Model Space. \n"));
        delete pCircle;
    }
    pModelSpace->close();
}

void CPolylineMarker::ProcessPolylineVertices(AcDbPolyline* pPline, const AcGeMatrix3d& xform)
{
    if (!pPline) 
    {
        CUtils::acutPrintf(_T("[WARNING] ProcessPolylineVertices: Invalid polyline pointer. \n"));
        return;
    }

    int numVerts = pPline->numVerts();
    CUtils::acutPrintf(_T("[INFO] ProcessPolylineVertices: Processing polyline with %d vertices. \n"), numVerts);

    for (int i = 0; i < numVerts; i++)
    {
        AcGePoint2d pt2d;
        if (pPline->getPointAt(i, pt2d) == Acad::eOk)
        {
            AcGePoint3d pt3d(pt2d.x, pt2d.y, 0.0);
            // Apply transformation if provided (for block references)
            pt3d.transformBy(xform);
            
            CUtils::acutPrintf(_T("[DEBUG] Vertex %d: Original(%.3f, %.3f) -> Transformed(%.3f, %.3f, %.3f) \n"), 
                       i, pt2d.x, pt2d.y, pt3d.x, pt3d.y, pt3d.z);
            
            DrawRedCircle(pt3d);
        }
        else
        {
            CUtils::acutPrintf(_T("[ERROR] ProcessPolylineVertices: Failed to get point at index %d. \n"), i);
        }
    }
}

void CPolylineMarker::ExecuteSelectionAndMark()
{
	CUtils::acutPrintf(_T("[INFO] 昇龙助手: 开始选择参照物，选择块或者多段线...\n  绘制直径：%.3f \n"), this->m_Diameter);

    ads_name ssName;
    // Allow user to select objects. 
    // NULL filters allow any object type, we filter in code.
    int res = acedSSGet(NULL, NULL, NULL, NULL, ssName);
    
    if (res != RTNORM)
    {
        CUtils::acutPrintf(_T("[INFO] ExecuteSelectionAndMark: No objects selected or selection cancelled.\n"));
        return;
    }

    long length = 0;
    acedSSLength(ssName, &length);
    CUtils::acutPrintf(_T("[INFO] ExecuteSelectionAndMark: Selected %ld entities.\n"), length);

	AcApDocument* pDoc = acDocManager->curDocument();

	Acad::ErrorStatus es = acDocManager->lockDocument(pDoc);
    if (es != Acad::eOk) {

        CUtils::acutPrintf(_T("Failed to lock the document... \n"));
        return;
    }

    for (long i = 0; i < length; i++)
    {
        ads_name entName;
        AcDbObjectId objId;
        
        if (acedSSName(ssName, i, entName) != RTNORM)
        {
            CUtils::acutPrintf(_T("[WARNING] ExecuteSelectionAndMark: Failed to get entity name at index %ld. \n"), i);
            continue;
        }

        if (acdbGetObjectId(objId, entName) != Acad::eOk)
        {
            CUtils::acutPrintf(_T("[WARNING] ExecuteSelectionAndMark: Failed to get Object ID at index %ld. \n"), i);
            continue;
        }

        AcDbEntity* pEnt = NULL;
        if (acdbOpenObject(pEnt, objId, AcDb::kForRead) != Acad::eOk)
        {
            CUtils::acutPrintf(_T("[WARNING] ExecuteSelectionAndMark: Failed to open entity at index %ld. \n"), i);
            continue;
        }

        CUtils::acutPrintf(_T("[DEBUG] Processing Entity Index %ld, Object ID: %ld \n"), i, objId.asOldId());

        // Case 1: Direct Polyline
        AcDbPolyline* pPline = AcDbPolyline::cast(pEnt);
        if (pPline)
        {
            CUtils::acutPrintf(_T("[INFO] Entity is a Polyline. \n"));
            ProcessPolylineVertices(pPline);
            pEnt->close();
            continue;
        }

        // Case 2: Block Reference
        AcDbBlockReference* pBlockRef = AcDbBlockReference::cast(pEnt);
        if (pBlockRef)
        {
            CUtils::acutPrintf(_T("[INFO] Entity is a Block Reference. \n"));
            AcDbObjectId blockDefId = pBlockRef->blockTableRecord();
            
            // Get transformation matrix before closing reference
            AcGeMatrix3d xform = pBlockRef->blockTransform();
            pEnt->close(); 

            // Open the Block Definition
            AcDbBlockTableRecord* pBlockDef = NULL;
            if (acdbOpenObject(pBlockDef, blockDefId, AcDb::kForRead) == Acad::eOk)
            {
                CUtils::acutPrintf(_T("[DEBUG] Opening Block Definition for iteration. \n"));
                AcDbBlockTableRecordIterator* pIter = NULL;
                if (pBlockDef->newIterator(pIter) == Acad::eOk)
                {
                    int subEntCount = 0;
                    for (pIter->start(); !pIter->done(); pIter->step())
                    {
                        AcDbEntity* pSubEnt = NULL;
                        if (pIter->getEntity(pSubEnt, AcDb::kForRead) == Acad::eOk)
                        {
                            // Check if sub-entity is a polyline
                            AcDbPolyline* pSubPline = AcDbPolyline::cast(pSubEnt);
                            if (pSubPline)
                            {
                                CUtils::acutPrintf(_T("[INFO] Found Polyline inside Block. \n"));
                                // Draw circles at vertices, transformed by block's position/scale/rotation
                                ProcessPolylineVertices(pSubPline, xform);
                                subEntCount++;
                            }
                            pSubEnt->close();
                        }
                    }
                    CUtils::acutPrintf(_T("[DEBUG] Processed %d sub-entities in block. \n"), subEntCount);
                    delete pIter;
                }
                else
                {
                    CUtils::acutPrintf(_T("[ERROR] Failed to create iterator for block definition. \n"));
                }
                pBlockDef->close();
            }
            else
            {
                CUtils::acutPrintf(_T("[ERROR] Failed to open block definition.\n"));
            }
        }
        else
        {
            CUtils::acutPrintf(_T("[DEBUG] Entity is neither Polyline nor Block Reference. Skipping.\n"));
            pEnt->close();
        }
    }

    acedSSFree(ssName);
    CUtils::acutPrintf(_T("\n[INFO] ExecuteSelectionAndMark: Processing complete. Red circles drawn at polyline vertices.\n"));

	acDocManager->unlockDocument(pDoc);

	acedPostCommandPrompt(); // 立即刷新提示符
}