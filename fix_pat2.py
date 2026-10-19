import io, json

f = r"D:\project\ies\cad\exploration\Debug\support\pat_define.json"
with io.open(f, "r", encoding="utf-8") as fp:
    data = json.load(fp)

fixes = {
    "素填土": "CQ素填土",
    "黏土": "CQ粘土",
    "粘土": "CQ粘土",
    "泥灰岩": "CQ泥灰岩",
    "页岩": "CQ页岩",
}

for row in data:
    name = row.get("岩性名称", "")
    if name in fixes:
        row["图例名称"] = fixes[name]

with io.open(f, "w", encoding="utf-8") as fp:
    json.dump(data, fp, ensure_ascii=False, indent=2)
print("done")
