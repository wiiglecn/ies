#pragma once

class PreSelectScale {
public:
    PreSelectScale();
    ~PreSelectScale();

    void selectRegion();
    void scaleObjects(const char* paperSize);
    void scaleObjects(double customWidth, double customHeight);
    void deleteOldObjects();

private:
    // Members to store the selected region and scaling factors
    AcDbObjectIdArray selectedObjects;
    double scaleFactor;
    double customWidth;
    double customHeight;
};