import json
from pathlib import Path
def main():
    structure = {
        "水平比例": 250,
        "垂直比例": 200,
        "水位线": 208.55,
        "标高": 203,
        "剖面组": [
            {
                "标题":"1-1#、1-2#住宅楼",
                "勘探组":[
                    'ZK1','ZK2','ZK3','ZK4','ZK6'
                ]
            },
            {
                "标题":"1-1#、1-2#住宅楼",
                "勘探组":[
                    'ZK2','ZK15','ZK4','ZK5'
                ]
            }
        ]
    }
    output_path = Path(__file__).with_name("section_view_structure.json")
    with output_path.open("w", encoding="utf-8") as file:
        json.dump(structure, file, ensure_ascii=False, indent=2)

    print(f"导出完成：{output_path}")
    
    pass
if __name__ == '__main__':
    main()