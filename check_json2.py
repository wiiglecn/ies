import json
with open(r'D:\project\ies\cad\exploration\Debug\project\钻探编录表.json','r',encoding='utf-8') as f:
    data = json.load(f)
zk1 = data['ZK1']
print("=== ZK1 top keys ===")
print(list(zk1.keys()))
print("\n=== layers[0] keys ===")
layer0 = zk1['地层列表'][0]
print(list(layer0.keys()))
print("\n=== layers[0] content ===")
print(json.dumps(layer0, ensure_ascii=False, indent=2))
