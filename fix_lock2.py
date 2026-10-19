import io

f = r"D:\project\ies\cad\exploration\features\CreateSectionView.cpp"
# 文件可能混合编码，用 errors=replace
with io.open(f, "r", encoding="gbk", errors="replace") as fp:
    content = fp.read()

# 替换文档锁代码块
old = '''void CCreateSectionView::Execute()
{
\t// 获取文档写锁（PostMessage 期间文档是读锁定的）
\tAcApDocument* pDoc = acDocManager->curDocument();
\tif (pDoc && (pDoc->lockMode() == Acad::kNoDocLock || pDoc->lockMode() == Acad::kReadDocLock))
\t\tacDocManager->lockDocument(pDoc, Acad::kWriteDocLock);

\tif (!prepare()) return;'''

new = '''void CCreateSectionView::Execute()
{
\tif (!prepare()) return;'''

if old in content:
    content = content.replace(old, new)
    print("replaced exact")
else:
    # 手动找
    lines = content.split('\n')
    for i, line in enumerate(lines[:150]):
        if "Execute()" in line:
            print(f"Execute at line {i+1}")
            # 删除接下来 6 行（文档锁）
            del lines[i+1:i+7]
            break
    content = '\n'.join(lines)

with io.open(f, "w", encoding="gbk", errors="replace") as fp:
    fp.write(content)
print("done")
