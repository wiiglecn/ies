import io

f = r"D:\project\ies\cad\exploration\features\CreateSectionView.cpp"
with io.open(f, "r", encoding="gbk") as fp:
    lines = fp.readlines()

# 去掉第 134-138 行（文档锁代码 + 空行）
# 找到 Execute() 函数开头
start = None
for i, line in enumerate(lines):
    if "void CCreateSectionView::Execute()" in line:
        start = i
        break

print(f"Execute at line {start+1}")
# 删除 start+1 到 start+6（文档锁代码）
del lines[start+1:start+7]

with io.open(f, "w", encoding="gbk") as fp:
    fp.writelines(lines)
print("done")
