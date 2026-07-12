# 加密解密 — 实施计划

## 功能概述

允许用户指定密码，对备份数据进行加密存储（AES 和/或 SM4 算法），还原时使用相同密码解密。

## 对应需求

详见 [docs/requirements.md](../requirements.md)（加密解密 / 每种算法 10 分）。

## 技术方案

### 架构设计

```
Encryptor 接口 (encrypt / decrypt)
    │
    ├── AesEncryptor     ← OpenSSL EVP (AES-256-GCM)
    └── Sm4Encryptor    ← OpenSSL 3.x EVP (SM4-CBC/SM4-GCM)
            │
            ▼
EncryptorFactory (根据算法名创建)
```

### 加密策略

| 参数 | AES 方案 | SM4 方案 |
|------|---------|---------|
| 算法 | AES-256-GCM | SM4-CBC / SM4-GCM |
| 密钥派生 | PBKDF2 (100000 iterations, SHA-256) | PBKDF2 (100000 iterations, SM3) |
| 密钥长度 | 256 位 | 128 位 |
| 认证 | GCM 内置认证标签 (16B) | GCM 或 HMAC-SM3 |
| 初始化向量 | 随机 12B (GCM) / 16B (CBC) | 随机 12B (GCM) / 16B (CBC) |

### 加密数据格式

```
┌──────────────────────────────────────┐
│      加密文件头部 (可变长度)           │
│  ├─ Magic: "BACKERENC\0" (10B)       │
│  ├─ 算法标识 (1B): 0=AES-256-GCM     │
│  │                          1=SM4    │
│  ├─ PBKDF2 salt (16B)                │
│  ├─ PBKDF2 iterations (4B)           │
│  ├─ IV / nonce (12B 或 16B)          │
│  └─ 认证标签 (16B)                   │
├──────────────────────────────────────┤
│      加密后的密文数据                  │
└──────────────────────────────────────┘
```

### 缓冲区加密

对打包/压缩后的数据进行加密，采用缓冲区接口（span → vector），每次处理一个数据块：

```
┌──────────┐    ┌──────────┐    ┌──────────┐
│ 打包数据 │ →  │ 压缩引擎  │ →  │ 加密引擎  │ → 备份文件
└──────────┘    └──────────┘    └──────────┘
                                     ↑
                                用户密码 (交互式/参数)
```

加密器缓存自身状态（EVP_CIPHER_CTX 持有 IV 和加密上下文），支持多次调用 `encrypt()` 处理流式数据：第一次传头部，后续传数据块，最后传空数据触发 finalize。

## 实施步骤

| 步骤 | 内容 | 产出 | 状态 |
|------|------|------|------|
| 1. Encryptor 接口 | 定义 `encrypt()` / `decrypt()` 缓冲区接口 | `encryptor.h` | ✅ |
| 2. EncryptorFactory | 工厂类注册机制 | `build_encryptor.h/cpp` | ✅ |
| 3. 密钥派生 | PBKDF2 (密码→密钥) | `key_derivation.h/cpp` | ✅ |
| 4. 加密头部格式 | 序列化/反序列化加密参数 | `encrypted_format.h` | ✅ |
| 5. AES-256-GCM 实现 | OpenSSL EVP 封装 | `openssl_encryptor.cpp` | ✅ |
| 6. SM4 实现 (可选) | OpenSSL 3.x EVP SM4-CBC 封装 | `openssl_encryptor.cpp` | ✅ |
| 7. 密码输入 | 交互式 (不回显) + `--password` 参数 | `commands.cpp` | ✅ |
| 8. 密码确认 | 加密时要求输入两次密码 | `commands.cpp` | ✅ |
| 9. 解密验证 | 解密时验证 Magic + 认证标签 | `openssl_encryptor.cpp` | ✅ |
| 10. CLI 集成 | `--encrypt aes256/sm4`, `--password` | `main.cpp`, `commands.cpp` | ✅ |
| 11. 单元测试 | 加密→解密→验证原始数据 | `tests/crypto/` | ✅ |

## 关键接口

```cpp
class Encryptor {
public:
    virtual ~Encryptor() = default;

    // 加密：输入 span → 加密 → 输出追加到 vector
    virtual std::expected<void, ErrorCode> encrypt(
        std::span<char const> input,
        std::vector<char>& output,
        std::string_view password) = 0;

    // 解密：输入 span → 解密 → 输出追加到 vector
    virtual std::expected<void, ErrorCode> decrypt(
        std::span<char const> input,
        std::vector<char>& output,
        std::string_view password) = 0;

    virtual std::string_view name() const noexcept = 0;
};

// 密钥派生
class KeyDerivation {
public:
    struct DerivedKey {
        std::vector<char> key;
        std::vector<char> salt;
    };

    static DerivedKey deriveKey(std::string_view password,
                                std::span<char const> salt,
                                int iterations, size_t keyLength);

    static constexpr int kDefaultIterations = 100000;
    static constexpr size_t kSaltLength = 16;
};

// 交互式密码输入
std::optional<std::string> promptPassword(bool confirm);
```

## 测试策略

| 场景 | 方法 | 验证点 |
|------|------|--------|
| 基本加密解密 | AES 加密→解密 | 输出 == 输入 |
| 密码错误 | 使用错误密码解密 | 认证失败，返回错误 |
| 损坏密文 | 修改密文字节 | 认证标签校验失败 |
| 空数据 | 加密空数据 | 解密后为空 |
| 大文件 (1GiB+) | 流式加密大数据 | 无 OOM，结果一致 |
| SM4 模式 | SM4-CBC / SM4-GCM 加密解密 (可选) | 正确加解密 |
| 密码特殊字符 | 含空格/中文密码 | 正确处理 UTF-8 |
| 管道组合 | Packer → Compressor → Encryptor | 完整管道工作 |

## 安全注意事项

- **绝不在日志中打印密码或密钥**
- 密码输入时关闭回显 (`tcsetattr()` 或 `getpass()`)
- PBKDF2 迭代次数不宜过低（≥100000）
- 每次加密使用随机 salt 和 random IV
- 解密时先验证认证标签，标签不通过绝不输出数据
- 内存中的密钥使用完后尽量清零（`OPENSSL_cleanse()` / `explicit_bzero()`）

## 关键文件

```
src/crypto/
├── encryptor.h                 # Encryptor 接口
├── encryptor_factory.h
├── encryptor_factory.cpp
├── openssl_encryptor.h
├── openssl_encryptor.cpp       # AES + SM4 实现
├── key_derivation.h
├── key_derivation.cpp
└── encrypted_format.h          # 文件头部格式定义
src/cli/
└── commands.cpp                # --encrypt, --password 参数
tests/crypto/
├── openssl_encryptor_test.cpp
└── key_derivation_test.cpp
CMakeLists.txt                  # find_package(OpenSSL)
```

## 依赖安装

```bash
# Ubuntu / Debian
sudo apt install libssl-dev

# Fedora / RHEL
sudo dnf install openssl-devel
```

## 预计工作量

- 代码行数：~800 行（含测试）
- 开发周期：4-6 天
- 依赖：OpenSSL (libcrypto)
