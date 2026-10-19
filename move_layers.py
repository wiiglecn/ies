import io

f = r"D:\project\ies\cad\exploration\features\CreateSectionView.cpp"
with io.open(f, "r", encoding="gbk", errors="replace") as fp:
    lines = fp.readlines()

# 找图层创建循环和 startTrans 的位置
layer_start = None
starttrans_line = None
for i, line in enumerate(lines):
    if "for (int i = 0; i < nLayerCount" in line:
        layer_start = i
    if "if (this->startTrans()) return;" in line:
        starttrans_line = i
        break

print(f"layer loop at {layer_start+1}, startTrans at {starttrans_line+1}")

# 删除图层创建循环（layer_start 到 layer_start+9，包括闭合 }）
# 找循环结束的 }
layer_end = layer_start
brace_count = 0
for i in range(layer_start, len(lines)):
    brace_count += lines[i].count('{') - lines[i].count('}')
    if brace_count == 0 and i > layer_start:
        layer_end = i
        break

print(f"layer loop ends at {layer_end+1}")

# 提取循环代码
layer_code = lines[layer_start:layer_end+1]

# 删除循环
del lines[layer_start:layer_end+1]

# 找新的 startTrans 行号（删除后行号变了）
for i, line in enumerate(lines):
    if "if (this->startTrans()) return;" in line:
        starttrans_line = i
        break

# 在 startTrans 之后插入循环
insert_at = starttrans_line + 1
lines = lines[:insert_at] + layer_code + lines[insert_at:]

with io.open(f, "w", encoding="gbk", errors="replace") as fp:
    fp.writelines(lines)
print("done")
