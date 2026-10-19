import io

f = r"D:\project\ies\cad\exploration\src\Utils_ext_01.cpp"
with io.open(f, "r", encoding="gbk") as fp:
    lines = fp.readlines()

new_func = '''AcDbObjectId CUtils::EnsureLayer(const ACHAR* pszLayerName, bool bSetCurrent)
{
    if (pszLayerName == NULL || pszLayerName[0] == _T('\\0'))
        return AcDbObjectId::kNull;

    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (pDb == NULL) return AcDbObjectId::kNull;

    // 直接以写模式打开图层表（避免先读后写导致 320 锁冲突）
    AcDbLayerTable* pLyrTbl = NULL;
    Acad::ErrorStatus st = pDb->getLayerTable(pLyrTbl, AcDb::kForWrite);
    if (st != Acad::eOk) { CUtils::acutPrintf(_T("[LAYER DEBUG] kForWrite fail: %d, %s\\n"), (int)st, pszLayerName); return AcDbObjectId::kNull; }

    AcDbObjectId layerId = AcDbObjectId::kNull;
    if (pLyrTbl->getAt(pszLayerName, layerId) == Acad::eOk && !layerId.isNull())
    {
        pLyrTbl->close();
        if (bSetCurrent && pDb->clayer() != layerId) pDb->setClayer(layerId);
        return layerId;
    }

    AcDbLayerTableRecord* pLyrRec = new AcDbLayerTableRecord;
    pLyrRec->setName(pszLayerName);
    AcDbObjectId newId = AcDbObjectId::kNull;
    if (pLyrTbl->add(pLyrRec) == Acad::eOk) { newId = pLyrRec->objectId(); pLyrRec->close(); }
    else delete pLyrRec;
    pLyrTbl->close();

    if (!newId.isNull() && bSetCurrent) pDb->setClayer(newId);
    return newId;
}
'''

# 找到函数开始和结束
start = None
end = None
for i, line in enumerate(lines):
    if "AcDbObjectId CUtils::EnsureLayer" in line:
        start = i
    if start is not None and i > start and line.strip() == "}" and i > start + 5:
        end = i
        break

print(f"replace lines {start+1} to {end+1}")
result = lines[:start] + [new_func] + lines[end+1:]

with io.open(f, "w", encoding="gbk") as fp:
    fp.writelines(result)
print("done")
