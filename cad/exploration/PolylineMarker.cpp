// coding: gb18030
#include "StdAfx.h"
#include "PolylineMarker.h"
#include "dbents.h"
#include "geassign.h"
#include "acutads.h"
#include "arxHeaders.h"

CPolylineMarker::CPolylineMarker()
{
}

CPolylineMarker::~CPolylineMarker()
{
}

void CPolylineMarker::DrawRedCircle(const AcGePoint3d& center)
{
    acutPrintf(_T("\n[DEBUG] DrawRedCircle: Center(%.3f, %.3f, %.3f)"), center.x, center.y, center.z);

    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) 
    {
        acutPrintf(_T("\n[ERROR] DrawRedCircle: Failed to get working database."));
        return;
    }

    AcDbBlockTable* pBlockTable = NULL;
    AcDbBlockTableRecord* pModelSpace = NULL;

    // Open Model Space for write
    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk)
    {
        acutPrintf(_T("\n[ERROR] DrawRedCircle: Failed to open Block Table."));
        return;
    }
    
    if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForWrite) != Acad::eOk)
    {
        acutPrintf(_T("\n[ERROR] DrawRedCircle: Failed to open Model Space for write."));
        pBlockTable->close();
        return;
    }
    pBlockTable->close();

    // Create Circle
    AcDbCircle* pCircle = new AcDbCircle();
    pCircle->setCenter(center);
    pCircle->setRadius(1.5); // Adjust radius as needed
    pCircle->setColorIndex(1); // Red color

    // Add to Model Space
    AcDbObjectId circleId;
    if (pModelSpace->appendAcDbEntity(circleId, pCircle) == Acad::eOk)
    {
        acutPrintf(_T("\n[INFO] DrawRedCircle: Circle created successfully."));
        pCircle->close();
    }
    else
    {
        acutPrintf(_T("\n[ERROR] DrawRedCircle: Failed to append circle to Model Space."));
        delete pCircle;
    }
    pModelSpace->close();
}

void CPolylineMarker::ProcessPolylineVertices(AcDbPolyline* pPline, const AcGeMatrix3d& xform)
{
    if (!pPline) 
    {
        acutPrintf(_T("\n[WARNING] ProcessPolylineVertices: Invalid polyline pointer."));
        return;
    }

    int numVerts = pPline->numVerts();
    acutPrintf(_T("\n[INFO] ProcessPolylineVertices: Processing polyline with %d vertices."), numVerts);

    for (int i = 0; i < numVerts; i++)
    {
        AcGePoint2d pt2d;
        if (pPline->getPointAt(i, pt2d) == Acad::eOk)
        {
            AcGePoint3d pt3d(pt2d.x, pt2d.y, 0.0);
            // Apply transformation if provided (for block references)
            pt3d.transformBy(xform);
            
            acutPrintf(_T("\n[DEBUG] Vertex %d: Original(%.3f, %.3f) -> Transformed(%.3f, %.3f, %.3f)"), 
                       i, pt2d.x, pt2d.y, pt3d.x, pt3d.y, pt3d.z);
            
            DrawRedCircle(pt3d);
        }
        else
        {
            acutPrintf(_T("\n[ERROR] ProcessPolylineVertices: Failed to get point at index %d."), i);
        }
    }
}

void CPolylineMarker::ExecuteSelectionAndMark()
{
    acutPrintf(_T("\n[INFO] ExecuteSelectionAndMark: Starting selection..."));

    ads_name ssName;
    // Allow user to select objects. 
    // NULL filters allow any object type, we filter in code.
    int res = acedSSGet(NULL, NULL, NULL, NULL, ssName);
    
    if (res != RTNORM)
    {
        acutPrintf(_T("\n[INFO] ExecuteSelectionAndMark: No objects selected or selection cancelled."));
        return;
    }

    long length = 0;
    acedSSLength(ssName, &length);
    acutPrintf(_T("\n[INFO] ExecuteSelectionAndMark: Selected %ld entities."), length);

	AcApDocument* pDoc = acDocManager->curDocument();

	Acad::ErrorStatus es = acDocManager->lockDocument(pDoc);
    if (es != Acad::eOk) {
        acedAlert(_T("Failed to lock the document..."));
        return;
    }

    for (long i = 0; i < length; i++)
    {
        ads_name entName;
        AcDbObjectId objId;
        
        if (acedSSName(ssName, i, entName) != RTNORM)
        {
            acutPrintf(_T("\n[WARNING] ExecuteSelectionAndMark: Failed to get entity name at index %ld."), i);
            continue;
        }

        if (acdbGetObjectId(objId, entName) != Acad::eOk)
        {
            acutPrintf(_T("\n[WARNING] ExecuteSelectionAndMark: Failed to get Object ID at index %ld."), i);
            continue;
        }

        AcDbEntity* pEnt = NULL;
        if (acdbOpenObject(pEnt, objId, AcDb::kForRead) != Acad::eOk)
        {
            acutPrintf(_T("\n[WARNING] ExecuteSelectionAndMark: Failed to open entity at index %ld."), i);
            continue;
        }

        acutPrintf(_T("\n[DEBUG] Processing Entity Index %ld, Object ID: %ld"), i, objId.asOldId());

        // Case 1: Direct Polyline
        AcDbPolyline* pPline = AcDbPolyline::cast(pEnt);
        if (pPline)
        {
            acutPrintf(_T("\n[INFO] Entity is a Polyline."));
            ProcessPolylineVertices(pPline);
            pEnt->close();
            continue;
        }

        // Case 2: Block Reference
        AcDbBlockReference* pBlockRef = AcDbBlockReference::cast(pEnt);
        if (pBlockRef)
        {
            acutPrintf(_T("\n[INFO] Entity is a Block Reference."));
            AcDbObjectId blockDefId = pBlockRef->blockTableRecord();
            
            // Get transformation matrix before closing reference
            AcGeMatrix3d xform = pBlockRef->blockTransform();
            pEnt->close(); 

            // Open the Block Definition
            AcDbBlockTableRecord* pBlockDef = NULL;
            if (acdbOpenObject(pBlockDef, blockDefId, AcDb::kForRead) == Acad::eOk)
            {
                acutPrintf(_T("\n[DEBUG] Opening Block Definition for iteration."));
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
                                acutPrintf(_T("\n[INFO] Found Polyline inside Block."));
                                // Draw circles at vertices, transformed by block's position/scale/rotation
                                ProcessPolylineVertices(pSubPline, xform);
                                subEntCount++;
                            }
                            pSubEnt->close();
                        }
                    }
                    acutPrintf(_T("\n[DEBUG] Processed %d sub-entities in block."), subEntCount);
                    delete pIter;
                }
                else
                {
                    acutPrintf(_T("\n[ERROR] Failed to create iterator for block definition."));
                }
                pBlockDef->close();
            }
            else
            {
                acutPrintf(_T("\n[ERROR] Failed to open block definition."));
            }
        }
        else
        {
            acutPrintf(_T("\n[DEBUG] Entity is neither Polyline nor Block Reference. Skipping."));
            pEnt->close();
        }
    }

    acedSSFree(ssName);
    acutPrintf(_T("\n[INFO] ExecuteSelectionAndMark: Processing complete. Red circles drawn at polyline vertices."));

	acDocManager->unlockDocument(pDoc);
}