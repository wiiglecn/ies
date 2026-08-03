#include "PreSelectScale.h"
#include "PaperSizes.h"
#include "ScaleUtils.h"
#include "acdb.h"
#include "dbents.h"
#include "geassign.h"
#include "gept3d.h"
#include "dbsymtb.h"
#include "dbapserv.h"

void PreSelectScale::selectRegion()
{
    // Implementation for selecting a region in the drawing
    // This will involve user interaction to define the area to scale
}

void PreSelectScale::scaleObjects(const CString& paperSize)
{
    double scaleFactor = calculateScaleFactor(paperSize);
    if (scaleFactor <= 0.0) {
        acutPrintf(_T("Invalid scale factor.\n"));
        return;
    }

    // Iterate through selected objects and scale them
    for (auto& obj : selectedObjects) {
        // Scale the object based on the scale factor
        scaleObject(obj, scaleFactor);
    }

    // Redraw the scaled objects
    redrawObjects(selectedObjects);
}

void PreSelectScale::deleteOldObjects()
{
    for (auto& obj : selectedObjects) {
        // Delete the old object from the database
        obj->erase();
    }
    selectedObjects.clear();
}

double PreSelectScale::calculateScaleFactor(const CString& paperSize)
{
    PaperSizes sizes;
    return sizes.getScaleFactor(paperSize);
}

void PreSelectScale::scaleObject(AcDbEntity* obj, double scaleFactor)
{
    // Scale the object by the given scale factor
    if (obj) {
        obj->scaleBy(scaleFactor);
    }
}