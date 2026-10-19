import io

f = r"D:\project\ies\cad\exploration\features\CreateSectionView.cpp"
with io.open(f, "r", encoding="gbk", errors="replace") as fp:
    lines = fp.readlines()

# 在 Execute() { 后插入文档锁代码
for i, line in enumerate(lines):
    if "void CCreateSectionView::Execute()" in line:
        insert_at = i + 1
        lock_code = [
            "\t// 获取文档写锁（PostMessage 期间文档是读锁定的）\n",
            "\tAcApDocument* pDoc = acDocManager->curDocument();\n",
            "\tif (pDoc && pDoc->lockMode() == AcAp::kRead)\n",
            "\t\tacDocManager->lockDocument(pDoc, AcAp::kWrite);\n",
            "\n",
        ]
        lines = lines[:insert_at] + lock_code + lines[insert_at:]
        break

# 在 #include "PatHatchLoader.h" 后加 #include <acdocman.h>
for i, line in enumerate(lines):
    if "PatHatchLoader.h" in line:
        lines.insert(i + 1, "#include <acdocman.h>\n")
        break

with io.open(f, "w", encoding="gbk", errors="replace") as fp:
    fp.writelines(lines)
print("done")
