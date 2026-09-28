from pathlib import Path


def print_tree(folder: Path, level: int = 0):
    print("    " * level + folder.name + "/")

    subfolders = sorted(
        (item for item in folder.iterdir() if item.is_dir()),
        key=lambda item: item.name.lower(),
    )

    for subfolder in subfolders:
        print_tree(subfolder, level + 1)


def test():
    path = Path(r"D:\project\me\test\ObjectARX-SDK\2005-R16.1\samples")

    if not path.is_dir():
        print(f"目录不存在：{path}")
        return

    print_tree(path)


if __name__ == "__main__":
    test()