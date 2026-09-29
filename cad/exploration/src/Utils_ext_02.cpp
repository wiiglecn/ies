#include "StdAfx.h"
#include "Utils.h"
#include "arxHeaders.h"
#include <wincrypt.h>
#include <string>
#include <fstream>
#include <vector>
#include <algorithm>
#include "cJSON.h"

void CUtils::replaceBlockText(AcDbObjectId blkDefId, LPCTSTR pszPlaceholder, LPCTSTR pszNewVal)
{
    AcDbBlockTableRecord* pBTR = NULL;
    if (acdbOpenObject(pBTR, blkDefId, AcDb::kForRead) != Acad::eOk)
        return;

    AcDbBlockTableRecordIterator* pIter = NULL;
    if (pBTR->newIterator(pIter) == Acad::eOk)
    {
        for (; !pIter->done(); pIter->step())
        {
            AcDbEntity* pEnt = NULL;
            if (pIter->getEntity(pEnt, AcDb::kForWrite) != Acad::eOk)
                continue;

            AcDbText* pText = AcDbText::cast(pEnt);
            AcDbMText* pMText = AcDbMText::cast(pEnt);
            AcDbBlockReference* pNested = AcDbBlockReference::cast(pEnt);

            if (pText != NULL)
            {
                CString s(pText->textString());
                if (s.Replace(pszPlaceholder, pszNewVal) > 0)
                    pText->setTextString(s);
            }
            else if (pMText != NULL)
            {
                CString s(pMText->contents());
                if (s.Replace(pszPlaceholder, pszNewVal) > 0)
                    pMText->setContents(s);
            }
            else if (pNested != NULL)
            {
                // Ç¶Ì×¿é£ºµÝ¹é½øÈ¥Ìæ»»
                replaceBlockText(pNested->blockTableRecord(), pszPlaceholder, pszNewVal);
            }
            pEnt->close();
        }
        delete pIter;
    }
    pBTR->close();
}