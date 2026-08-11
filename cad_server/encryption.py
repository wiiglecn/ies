import ctypes
import ctypes.wintypes as wintypes
import hashlib
import os

# Windows CryptoAPI 常量
PROV_RSA_AES = 24
CRYPT_VERIFYCONTEXT = 0xF0000000
CALG_AES_256 = 0x00006610
CALG_SHA_256 = 0x0000800C
KP_IV = 1
PLAINTEXTKEYBLOB = 0x8
CUR_BLOB_VERSION = 2

advapi32 = ctypes.WinDLL('advapi32', use_last_error=True)

advapi32.CryptAcquireContextW.argtypes = [
    ctypes.POINTER(wintypes.HANDLE), wintypes.LPCWSTR, wintypes.LPCWSTR,
    wintypes.DWORD, wintypes.DWORD
]
advapi32.CryptAcquireContextW.restype = wintypes.BOOL

advapi32.CryptReleaseContext.argtypes = [wintypes.HANDLE, wintypes.DWORD]
advapi32.CryptReleaseContext.restype = wintypes.BOOL

advapi32.CryptImportKey.argtypes = [
    wintypes.HANDLE, ctypes.POINTER(ctypes.c_ubyte), wintypes.DWORD,
    wintypes.HANDLE, wintypes.DWORD, ctypes.POINTER(wintypes.HANDLE)
]
advapi32.CryptImportKey.restype = wintypes.BOOL

advapi32.CryptSetKeyParam.argtypes = [
    wintypes.HANDLE, wintypes.DWORD, ctypes.POINTER(ctypes.c_ubyte), wintypes.DWORD
]
advapi32.CryptSetKeyParam.restype = wintypes.BOOL

advapi32.CryptEncrypt.argtypes = [
    wintypes.HANDLE, wintypes.HANDLE, wintypes.BOOL, wintypes.DWORD,
    ctypes.POINTER(ctypes.c_ubyte), ctypes.POINTER(wintypes.DWORD), wintypes.DWORD
]
advapi32.CryptEncrypt.restype = wintypes.BOOL

advapi32.CryptDecrypt.argtypes = [
    wintypes.HANDLE, wintypes.HANDLE, wintypes.BOOL, wintypes.DWORD,
    ctypes.POINTER(ctypes.c_ubyte), ctypes.POINTER(wintypes.DWORD)
]
advapi32.CryptDecrypt.restype = wintypes.BOOL

advapi32.CryptDestroyKey.argtypes = [wintypes.HANDLE]
advapi32.CryptDestroyKey.restype = wintypes.BOOL


def _raise_winerror(msg):
    code = ctypes.get_last_error()
    raise OSError(f"{msg} (error: {code})")


def _acquire_context():
    """CryptAcquireContext: 获取 CSP 上下文句柄"""
    h_prov = wintypes.HANDLE()
    if not advapi32.CryptAcquireContextW(
        ctypes.byref(h_prov), None, None, PROV_RSA_AES, CRYPT_VERIFYCONTEXT
    ):
        _raise_winerror("CryptAcquireContext 失败")
    return h_prov


def _release_context(h_prov):
    """CryptReleaseContext: 释放 CSP 上下文"""
    advapi32.CryptReleaseContext(h_prov, 0)


def _import_aes_key(h_prov, key_bytes):
    """通过 PLAINTEXTKEYBLOB 导入 AES-256 密钥"""

    if len(key_bytes) != 32:
        raise ValueError(
            f"AES-256 key 必须是 32 字节，当前是 {len(key_bytes)} 字节"
        )

    # 12 字节头 + 32 字节 AES Key
    blob = bytearray(12 + len(key_bytes))

    # BLOBHEADER
    blob[0] = PLAINTEXTKEYBLOB
    blob[1] = CUR_BLOB_VERSION

    # reserved: blob[2:4] = 0

    # ALG_ID = CALG_AES_256
    blob[4:8] = CALG_AES_256.to_bytes(4, "little")

    # DWORD dwKeySize = 32
    blob[8:12] = len(key_bytes).to_bytes(4, "little")

    # AES key
    blob[12:] = key_bytes

    print("key len :", len(key_bytes))
    print("blob len:", len(blob))
    print("blob    :", blob.hex())

    # 注意：这里必须和 argtypes 的 POINTER(c_byte) 对应
    c_blob = (ctypes.c_ubyte * len(blob))(*blob)

    h_key = wintypes.HANDLE()

    if not advapi32.CryptImportKey(
        h_prov,
        c_blob,
        len(blob),
        0,
        0,
        ctypes.byref(h_key)
    ):
        err = ctypes.get_last_error()
        raise ctypes.WinError(err)

    return h_key


def encrypt_file(password: str, input_path: str, output_path: str = None) -> bytes:
    """
    使用 Windows CryptoAPI 加密文本文件。

    调用链: CryptAcquireContext -> CryptImportKey -> CryptSetKeyParam(IV) -> CryptEncrypt

    输出格式: [16字节 IV][AES-256-CBC 密文(含PKCS7填充)]

    Args:
        password:    加密密码
        input_path:  输入文本文件路径 (UTF-8)
        output_path: 输出加密文件路径；None 时仅返回字节

    Returns:
        加密后的字节 (IV + 密文)
    """
    with open(input_path, 'r', encoding='utf-8') as f:
        plaintext = f.read().encode('utf-8')

    key_bytes = hashlib.sha256(password.encode('utf-8')).digest()
    iv = os.urandom(16)

    h_prov = _acquire_context()
    try:
        h_key = _import_aes_key(h_prov, key_bytes)
        try:
            c_iv = (ctypes.c_ubyte * 16)(*iv)
            if not advapi32.CryptSetKeyParam(h_key, KP_IV, c_iv, 0):
                _raise_winerror("CryptSetKeyParam(KP_IV) 失败")

            buf_len = len(plaintext) + 16
            c_buf = (ctypes.c_ubyte * buf_len)(*plaintext)
            c_data_len = wintypes.DWORD(len(plaintext))
            c_buf_len = wintypes.DWORD(buf_len)

            if not advapi32.CryptEncrypt(h_key, 0, True, 0, c_buf, ctypes.byref(c_data_len), c_buf_len):
                _raise_winerror("CryptEncrypt 失败")

            encrypted = bytes(c_buf[:c_data_len.value])
        finally:
            advapi32.CryptDestroyKey(h_key)
    finally:
        _release_context(h_prov)

    result = iv + encrypted
    if output_path:
        with open(output_path, 'wb') as f:
            f.write(result)
    return result


def decrypt_file(password: str, input_path: str, output_path: str = None) -> str:
    """
    使用 Windows CryptoAPI 解密文件。

    调用链: CryptAcquireContext -> CryptImportKey -> CryptSetKeyParam(IV) -> CryptDecrypt

    Args:
        password:    解密密码
        input_path:  加密文件路径
        output_path: 输出解密文本路径；None 时仅返回字符串

    Returns:
        解密后的明文字符串
    """
    with open(input_path, 'rb') as f:
        data = f.read()

    iv = data[:16]
    ciphertext = data[16:]
    key_bytes = hashlib.sha256(password.encode('utf-8')).digest()

    h_prov = _acquire_context()
    try:
        h_key = _import_aes_key(h_prov, key_bytes)
        try:
            c_iv = (ctypes.c_ubyte * 16)(*iv)
            if not advapi32.CryptSetKeyParam(h_key, KP_IV, c_iv, 0):
                _raise_winerror("CryptSetKeyParam(KP_IV) 失败")

            buf_len = len(ciphertext) + 16
            c_buf = (ctypes.c_ubyte * buf_len)(*(ciphertext), *([0] * 16))
            c_data_len = wintypes.DWORD(len(ciphertext))

            if not advapi32.CryptDecrypt(h_key, 0, True, 0, c_buf, ctypes.byref(c_data_len)):
                _raise_winerror("CryptDecrypt 失败")

            plaintext = bytes(c_buf[:c_data_len.value])
        finally:
            advapi32.CryptDestroyKey(h_key)
    finally:
        _release_context(h_prov)

    result = plaintext.decode('utf-8')
    if output_path:
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(result)
    return result


def main():
    test_file = "../cad/exploration/res/chat.txt"
    encrypted_file = "chat.bin"
    decrypted_file = "chat2.txt"
    password = "KS123456"

    # with open(test_file, 'w', encoding='utf-8') as f:
    #     f.write("这是一个测试文本文件。\nHello, Windows CryptoAPI!\n")

    print("原始内容:")
    with open(test_file, 'r', encoding='utf-8') as f:
        print(f.read())

    encrypt_file(password, test_file, encrypted_file)
    print(f"加密完成 -> {encrypted_file}")

    plaintext = decrypt_file(password, encrypted_file, decrypted_file)
    print(f"解密完成 -> {decrypted_file}")
    print(f"解密后内容:")
    print(plaintext)

    # for p in [test_file, encrypted_file, decrypted_file]:
    #     if os.path.exists(p):
    #         os.remove(p)
    # print("测试文件已清理。")


if __name__ == "__main__":
    main()
