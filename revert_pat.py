import io, json

f = r"D:\project\ies\cad\exploration\Debug\support\pat_define.json"
with io.open(f, "r", encoding="utf-8") as fp:
    data = json.load(fp)

# 恢复原始映射
restore = {
    "素填土": "GKstt",
    "黏土": "GKnxt",
    "粘土": "GKnxt",
    "泥灰岩": "GKnhy1",
    "页岩": "GKyy",
}

for row in data:
    name = row.get("岩性名称", "")
    if name in restore:
        row["图例名称"] = restore[name]

with io.open(f, "w", encoding="utf-8") as fp:
    json.dump(data, fp, ensure_ascii=False, indent=2)
print("reverted")
