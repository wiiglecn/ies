#include "stdafx.h"
#include "ToolBlock.h"
#include <queue>
#include "cJSON.h"
#include "arxHeaders.h"
#include "Utils.h"

ToolBlock::ToolBlock(void):ToolDrawBase()
{
}

ToolBlock::~ToolBlock(void)
{
}
ToolBlock& ToolBlock::getInstance()
{
    static ToolBlock instance;
    return instance;
}

// block_list: 列出当前图形中所有块定义
void ToolBlock::block_list(AiToolCommandData* data)
{
	if (actionBefore() != 0) return;

	AcDbBlockTable* pBlockTable = NULL;
	if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
		actionEnd();
		return;
	}

	AcDbBlockTableIterator* pIter = NULL;
	if (pBlockTable->newIterator(pIter) != Acad::eOk) {
		pBlockTable->close();
		actionEnd();
		return;
	}

	cJSON* pJsonArray = cJSON_CreateArray();

	for (; !pIter->done(); pIter->step()) {
		AcDbBlockTableRecord* pBTR = NULL;
		if (pIter->getRecord(pBTR, AcDb::kForRead) == Acad::eOk) {
			// 获取块名秿
			const ACHAR* pName = NULL;
			if (pBTR->getName(pName) == Acad::eOk && pName != NULL) {
				char utf8Name[256] = { 0 };
#ifdef _CAD2005
				strcpy(utf8Name, pName);
#else
				CUtils::TCharToUtf8(pName, utf8Name, sizeof(utf8Name));
#endif

				// 获取块表记录句柄
				AcDbHandle handle = pBTR->objectId().handle();
#ifdef _CAD2005
				ACHAR handleStr[20] = { 0 };
#else	
				TCHAR handleStr[20] = { 0 };
#endif
				handle.getIntoAsciiBuffer(handleStr);
				
#ifdef _CAD2005
				char* utf8Handle = handleStr;
#else
				char utf8Handle[32] = { 0 };
				CUtils::TCharToUtf8(handleStr, utf8Handle, sizeof(utf8Handle));
#endif
				// 是否为匿名块
				int isAnonymous = pBTR->isAnonymous();
				// 是否为布局坿
				int isLayout = pBTR->isLayout();

				cJSON* pJsonItem = cJSON_CreateObject();
				cJSON_AddStringToObject(pJsonItem, "name", utf8Name);
				cJSON_AddStringToObject(pJsonItem, "handle", utf8Handle);
				cJSON_AddNumberToObject(pJsonItem, "is_anonymous", isAnonymous);
				cJSON_AddNumberToObject(pJsonItem, "is_layout", isLayout);
				cJSON_AddItemToArray(pJsonArray, pJsonItem);
			}
			pBTR->close();
		}
	}

	delete pIter;
	pBlockTable->close();

	data->resultJson = pJsonArray;
	data->ret = 0;
	actionEnd();
}

// block_insert: 插入块参照到模型空间
void ToolBlock::block_insert(AiToolCommandData* data)
{
	if (actionBefore() != 0) return;

	cJSON* nameItem = cJSON_GetObjectItem(data->jsonRoot, "name");
	if (!nameItem || !cJSON_IsString(nameItem)) {
		actionEnd();
		return;
	}

	ACHAR blockName[256] = { 0 };
#ifdef _CAD2005
	strcpy(blockName,  nameItem->valuestring);
#else
	CUtils::utf8ToTChar(nameItem->valuestring, blockName, ARRAYSIZE(blockName));
#endif
	double x = cJSON_GetObjectItem(data->jsonRoot, "x")->valuedouble;
	double y = cJSON_GetObjectItem(data->jsonRoot, "y")->valuedouble;

	double scale = 1.0;
	cJSON* scaleItem = cJSON_GetObjectItem(data->jsonRoot, "scale");
	if (scaleItem) scale = scaleItem->valuedouble;

	double rotation = 0.0;
	cJSON* rotItem = cJSON_GetObjectItem(data->jsonRoot, "rotation");
	if (rotItem) rotation = rotItem->valuedouble;

	// 查找块
	AcDbBlockTable* pBlockTable = NULL;
	if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
		actionEnd();
		return;
	}

	AcDbObjectId blockId;
	if (pBlockTable->getAt(blockName, blockId) != Acad::eOk) {
		pBlockTable->close();
		actionEnd();
		return;
	}
	pBlockTable->close();

	// 创建块
	AcGePoint3d insertPt(x, y, 0);
	AcDbBlockReference* pBlkRef = new AcDbBlockReference(insertPt, blockId);
	pBlkRef->setScaleFactors(scale);
	pBlkRef->setRotation(rotation);

	if (pModelSpace->appendAcDbEntity(pBlkRef) == Acad::eOk) {
		// 返回块参照的句柄
		AcDbHandle handle = pBlkRef->objectId().handle();
		ACHAR handleStr[20] = { 0 };
		handle.getIntoAsciiBuffer(handleStr);
#ifdef _CAD2005
		char* utf8Handle = handleStr;
#else
		char utf8Handle[32] = { 0 };
		CUtils::TCharToUtf8(handleStr, utf8Handle, sizeof(utf8Handle));
#endif
		cJSON* pResult = cJSON_CreateObject();
		cJSON_AddStringToObject(pResult, "handle", utf8Handle);
		data->resultJson = pResult;
		data->ret = 0;
	}
	pBlkRef->close();

	actionEnd();
}

// block_insert_with_attributes: 插入带属性的块参煿
void ToolBlock::block_insert_with_attributes(AiToolCommandData* data)
{
	if (actionBefore() != 0) return;

	cJSON* nameItem = cJSON_GetObjectItem(data->jsonRoot, "name");
	if (!nameItem || !cJSON_IsString(nameItem)) {
		actionEnd();
		return;
	}

	ACHAR blockName[256] = { 0 };
#ifdef _CAD2005
	strcpy(blockName, nameItem->valuestring);
#else
	CUtils::utf8ToTChar(nameItem->valuestring, blockName, ARRAYSIZE(blockName));
#endif
	double x = cJSON_GetObjectItem(data->jsonRoot, "x")->valuedouble;
	double y = cJSON_GetObjectItem(data->jsonRoot, "y")->valuedouble;

	double scale = 1.0;
	cJSON* scaleItem = cJSON_GetObjectItem(data->jsonRoot, "scale");
	if (scaleItem) scale = scaleItem->valuedouble;

	double rotation = 0.0;
	cJSON* rotItem = cJSON_GetObjectItem(data->jsonRoot, "rotation");
	if (rotItem) rotation = rotItem->valuedouble;

	cJSON* attrsObj = cJSON_GetObjectItem(data->jsonRoot, "attributes");

	// 查找块定
	AcDbBlockTable* pBlockTable = NULL;
	if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
		actionEnd();
		return;
	}

	AcDbObjectId blockId;
	if (pBlockTable->getAt(blockName, blockId) != Acad::eOk) {
		pBlockTable->close();
		actionEnd();
		return;
	}
	pBlockTable->close();

	// 创建块参煿
	AcGePoint3d insertPt(x, y, 0);
	AcDbBlockReference* pBlkRef = new AcDbBlockReference(insertPt, blockId);
	pBlkRef->setScaleFactors(scale);
	pBlkRef->setRotation(rotation);

	if (pModelSpace->appendAcDbEntity(pBlkRef) != Acad::eOk) {
		delete pBlkRef;
		actionEnd();
		return;
	}

	// 如果有属性，遍历块定义中的属性定义并创建属性参煿
	if (attrsObj && cJSON_IsObject(attrsObj)) {
		AcDbBlockTableRecord* pBTR = NULL;
		if (acdbOpenAcDbObject((AcDbObject*&)pBTR, blockId, AcDb::kForRead) == Acad::eOk) {
			AcDbBlockTableRecordIterator* pIter = NULL;
			if (pBTR->newIterator(pIter) == Acad::eOk) {
				for (; !pIter->done(); pIter->step()) {
					AcDbObject* pObj = NULL;
					if (pIter->getEntity((AcDbEntity*&)pObj, AcDb::kForRead) != Acad::eOk) continue;

					AcDbAttributeDefinition* pAttDef = AcDbAttributeDefinition::cast(pObj);
					if (pAttDef == NULL) {
						pObj->close();
						continue;
					}

					// 获取属性标记
					const ACHAR* pTag = pAttDef->tag();
#ifdef _CAD2005
					char* utf8Tag=const_cast<char*>(pTag);
#else
					char utf8Tag[128] = { 0 };
					CUtils::TCharToUtf8(pTag, utf8Tag, sizeof(utf8Tag));
#endif

					// 从JSON中查找对应的倿
					cJSON* valItem = cJSON_GetObjectItem(attrsObj, utf8Tag);

					AcDbAttribute* pAtt = new AcDbAttribute();
					pAtt->setTag(pAttDef->tag());
					pAtt->setFieldLength(pAttDef->fieldLength());

					if (valItem && cJSON_IsString(valItem)) {
#ifdef _CAD2005
						char* valStr=valItem->valuestring;
#else
						TCHAR valStr[512] = { 0 };
						CUtils::utf8ToTChar(valItem->valuestring, valStr, ARRAYSIZE(valStr));
#endif
						pAtt->setTextString(valStr);

					} else {
						// 使用默认值
						const ACHAR* pDefault = pAttDef->textString();
						if (pDefault) pAtt->setTextString(pDefault);
					}

					pAtt->setHorizontalMode(pAttDef->horizontalMode());
					pAtt->setVerticalMode(pAttDef->verticalMode());
					pAtt->setPosition(pAttDef->position());
					pAtt->setHeight(pAttDef->height());

					pBlkRef->appendAttribute(pAtt);
					pAtt->close();
					pObj->close();
				}
				delete pIter;
			}
			pBTR->close();
		}
	}

	// 返回块参照句柿
	AcDbHandle handle = pBlkRef->objectId().handle();
	ACHAR handleStr[20] = { 0 };
	handle.getIntoAsciiBuffer(handleStr);
#ifdef _CAD2005
	char* utf8Handle=handleStr;
#else
	char utf8Handle[32] = { 0 };
	CUtils::TCharToUtf8(handleStr, utf8Handle, sizeof(utf8Handle));
#endif
	cJSON* pResult = cJSON_CreateObject();
	cJSON_AddStringToObject(pResult, "handle", utf8Handle);
	data->resultJson = pResult;
	data->ret = 0;

	pBlkRef->close();
	actionEnd();
}

// block_get_attributes: 获取块参照的属怿
void ToolBlock::block_get_attributes(AiToolCommandData* data)
{
	if (actionBefore() != 0) return;

	AcDbObjectId objId = GetIdFromHandle(data->jsonRoot, "handle", pDb);
	if (objId.isNull()) { actionEnd(); return; }

	AcDbBlockReference* pBlkRef = NULL;
	if (acdbOpenAcDbObject((AcDbObject*&)pBlkRef, objId, AcDb::kForRead) != Acad::eOk) {
		actionEnd();
		return;
	}

	cJSON* pJsonObj = cJSON_CreateObject();

	AcDbObjectIterator* pAttIter = pBlkRef->attributeIterator();
	if (pAttIter != NULL) {
		for (; !pAttIter->done(); pAttIter->step()) {
			AcDbObjectId attId = pAttIter->objectId();

			AcDbAttribute* pAtt = NULL;
			if (acdbOpenAcDbObject((AcDbObject*&)pAtt, attId, AcDb::kForRead) == Acad::eOk) {
				const ACHAR* pTag = pAtt->tag();
#ifdef _CAD2005
				char* utf8Tag = const_cast<char*>(pTag);
#else
				char utf8Tag[128] = { 0 };
				CUtils::TCharToUtf8(pTag, utf8Tag, sizeof(utf8Tag));
#endif
				const ACHAR* pVal = pAtt->textString();
#ifdef _CAD2005
				char* utf8Val = const_cast<char*>(pVal);
#else
				char utf8Val[512] = { 0 };
				CUtils::TCharToUtf8(pVal, utf8Val, sizeof(utf8Val));
#endif
				cJSON_AddStringToObject(pJsonObj, utf8Tag, utf8Val);
				pAtt->close();
			}
		}
		delete pAttIter;
	}

	pBlkRef->close();

	data->resultJson = pJsonObj;
	data->ret = 0;
	actionEnd();
}

// block_update_attributes: 更新块参照的属怿
void ToolBlock::block_update_attributes(AiToolCommandData* data)
{
	if (actionBefore() != 0) return;

	AcDbObjectId objId = GetIdFromHandle(data->jsonRoot, "handle", pDb);
	if (objId.isNull()) { actionEnd(); return; }

	cJSON* attrsObj = cJSON_GetObjectItem(data->jsonRoot, "attributes");
	if (!attrsObj || !cJSON_IsObject(attrsObj)) {
		actionEnd();
		return;
	}

	AcDbBlockReference* pBlkRef = NULL;
	if (acdbOpenAcDbObject((AcDbObject*&)pBlkRef, objId, AcDb::kForWrite) != Acad::eOk) {
		actionEnd();
		return;
	}

	AcDbObjectIterator* pAttIter = pBlkRef->attributeIterator();
	if (pAttIter != NULL) {
		for (; !pAttIter->done(); pAttIter->step()) {
			AcDbObjectId attId = pAttIter->objectId();

			AcDbAttribute* pAtt = NULL;
			if (acdbOpenAcDbObject((AcDbObject*&)pAtt, attId, AcDb::kForWrite) == Acad::eOk) {
				const ACHAR* pTag = pAtt->tag();
#ifdef _CAD2005
				char* utf8Tag=const_cast<char*>(pTag);
#else
				char utf8Tag[128] = { 0 };
				CUtils::TCharToUtf8(pTag, utf8Tag, sizeof(utf8Tag));
#endif
				cJSON* valItem = cJSON_GetObjectItem(attrsObj, utf8Tag);
				if (valItem && cJSON_IsString(valItem)) {
					ACHAR valStr[512] = { 0 };
#ifdef _CAD2005
					strcpy(valStr, valItem->valuestring);
#else
					CUtils::utf8ToTChar(valItem->valuestring, valStr, ARRAYSIZE(valStr));
#endif
					pAtt->setTextString(valStr);
				}
				pAtt->close();
			}
		}
		delete pAttIter;
	}

	pBlkRef->close();

	data->ret = 0;
	actionEnd();
}

// block_define: 定义新块（创建空块定义）
void ToolBlock::block_define(AiToolCommandData* data)
{
	if (actionBefore() != 0) return;

	cJSON* nameItem = cJSON_GetObjectItem(data->jsonRoot, "name");
	if (!nameItem || !cJSON_IsString(nameItem)) {
		actionEnd();
		return;
	}

	ACHAR blockName[256] = { 0 };
#ifdef _CAD2005
	strcpy(blockName, nameItem->valuestring);
#else
	CUtils::utf8ToTChar(nameItem->valuestring, blockName, ARRAYSIZE(blockName));
#endif
	// 获取块表
	AcDbBlockTable* pBlockTable = NULL;
	if (pDb->getBlockTable(pBlockTable, AcDb::kForWrite) != Acad::eOk) {
		actionEnd();
		return;
	}

	// 检查块是否已存圿
	AcDbObjectId existingId;
	if (pBlockTable->getAt(blockName, existingId) == Acad::eOk) {
		// 块已存在，返回已有块的句柿
		AcDbHandle handle = existingId.handle();
		ACHAR handleStr[20] = { 0 };
		handle.getIntoAsciiBuffer(handleStr);
#ifdef _CAD2005
		char* utf8Handle=handleStr;
#else
		char utf8Handle[32] = { 0 };
		CUtils::TCharToUtf8(handleStr, utf8Handle, sizeof(utf8Handle));
#endif
		cJSON* pResult = cJSON_CreateObject();
		cJSON_AddStringToObject(pResult, "handle", utf8Handle);
		cJSON_AddNumberToObject(pResult, "created", 0);
		data->resultJson = pResult;
		data->ret = 0;

		pBlockTable->close();
		actionEnd();
		return;
	}

	// 创建新的块定乿
	AcDbBlockTableRecord* pBTR = new AcDbBlockTableRecord();
	pBTR->setName(blockName);

	// 获取基点（可选）
	double baseX = 0.0, baseY = 0.0;
	cJSON* bxItem = cJSON_GetObjectItem(data->jsonRoot, "base_x");
	cJSON* byItem = cJSON_GetObjectItem(data->jsonRoot, "base_y");
	if (bxItem) baseX = bxItem->valuedouble;
	if (byItem) baseY = byItem->valuedouble;
	pBTR->setOrigin(AcGePoint3d(baseX, baseY, 0));

	// 检查是否有实体列表需要添加到块定义中
	cJSON* entitiesArr = cJSON_GetObjectItem(data->jsonRoot, "entities");

	if (pBlockTable->add(pBTR) == Acad::eOk) {
		// 如果有实体列表，将指定实体复制到块定义中
		if (entitiesArr && cJSON_IsArray(entitiesArr)) {
			cJSON* handleItem = NULL;
			cJSON_ArrayForEach(handleItem, entitiesArr) {
				if (!cJSON_IsString(handleItem) || !handleItem->valuestring) continue;

				TCHAR hStr[32] = { 0 };
				CUtils::utf8ToTChar(handleItem->valuestring, hStr, ARRAYSIZE(hStr));
				AcDbHandle dbHandle(hStr);

				AcDbObjectId srcId;
				if (pDb->getAcDbObjectId(srcId, Adesk::kFalse, dbHandle) != Acad::eOk) continue;

				AcDbEntity* pSrcEnt = NULL;
				if (acdbOpenAcDbEntity(pSrcEnt, srcId, AcDb::kForRead) == Acad::eOk) {
					AcDbEntity* pClone = AcDbEntity::cast(pSrcEnt->clone());
					if (pClone) {
						pBTR->appendAcDbEntity(pClone);
						pClone->close();
					}
					pSrcEnt->close();
				}
			}
		}

		// 返回新块定义的句柿
		AcDbHandle handle = pBTR->objectId().handle();
		ACHAR handleStr[20] = { 0 };
		handle.getIntoAsciiBuffer(handleStr);
#ifdef _CAD2005
		char* utf8Handle=handleStr;
#else
		char utf8Handle[32] = { 0 };
		CUtils::TCharToUtf8(handleStr, utf8Handle, sizeof(utf8Handle));
#endif
		cJSON* pResult = cJSON_CreateObject();
		cJSON_AddStringToObject(pResult, "handle", utf8Handle);
		cJSON_AddNumberToObject(pResult, "created", 1);
		data->resultJson = pResult;
		data->ret = 0;
	} else {
		delete pBTR;
	}

	pBlockTable->close();
	actionEnd();
}
