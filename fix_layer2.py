import io

f = r"D:\project\ies\cad\exploration\src\Utils_ext_01.cpp"
with io.open(f, "r", encoding="gbk") as fp:
    lines = fp.readlines()

# 新函数结束在第 95 行 (索引94)，第 96 行空行，第 97 行开始是残留旧代码
# 找到新函数结束后的第一个非空行
start = None
for i, line in enumerate(lines):
    if line.strip() == "// 2." or "getLayerTable(pLyrTbl, AcDb::kForWrite)" in line and i > 95:
        start = i
        break

print(f"残留旧代码从第 {start+1} 行开始")
# 删除从 start 到下一个函数定义之前
end = None
for i in range(start, len(lines)):
    if lines[i].startswith("// ") and "图层" in lines[i] or "EraseLayerEntities" in lines[i]:
        end = i
        break

print(f"删到第 {end} 行")
result = lines[:95] + ["\n"] + lines[end:]

with io.open(f, "w", encoding="gbk") as fp:
    fp.writelines(result)
print("done")
