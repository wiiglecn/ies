import io

f = r"D:\project\ies\cad\exploration\features\CreateSectionView.cpp"
with io.open(f, "r", encoding="gbk", errors="replace") as fp:
    lines = fp.readlines()

# 删除文档锁代码（在 Execute() { 之后）
for i, line in enumerate(lines):
    if "AcApDocument* pDoc" in line:
        # 删除前面的注释行
        start = i - 1
        while "//" in lines[start] or lines[start].strip() == "":
            start -= 1
        # 删除 i 到 i+4（文档锁代码 + 空行）
        del lines[start+1:i+5]
        break

# 删除 #include <acdocman.h>
for i, line in enumerate(lines):
    if "acdocman.h" in line:
        del lines[i]
        break

with io.open(f, "w", encoding="gbk", errors="replace") as fp:
    fp.writelines(lines)
print("done")
