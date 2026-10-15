import json
with open(r'D:\project\ies\cad\exploration\Debug\project\钻探编录表.json','r',encoding='utf-8') as f:
    data = json.load(f)
zk1 = data['ZK1']
print('Total layers:', len(zk1['地层列表']))
for i, l in enumerate(zk1['地层列表']):
    print(f'Layer {i}: from={l["from"]}, to={l["to"]}, name={l["name"]}')
print('Full ZK1 data:')
print(json.dumps(zk1, ensure_ascii=False, indent=2))
