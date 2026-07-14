# 子系统四：加密子系统构件图描述

## 对应源文件

`subsystem-04-encryption.svg`

## 图概述

本图展示了加密子系统的内部结构，包含加密文件格式定义、密钥派生模块、Encryptor 抽象接口与工厂、以及基于 OpenSSL EVP 的实现类。

## 构件说明

### 加密格式定义（encrypted_format.h）

| 构件 | 类型 | 说明 |
|------|------|------|
| EncryptedHeader | 结构体（packed, 47 字节） | 加密文件头，固定布局：magic[10] = "BACKERENC\0"（魔术字，含 NUL 终止符）、algorithm（uint8，Algorithm 枚举值）、salt[16]（随机盐值，用于 PBKDF2）、iterations（uint32 LE，PBKDF2 迭代次数，默认 100000）、iv[16]（随机初始化向量） |
| Algorithm | 枚举（uint8_t） | 加密算法枚举：kAes256Gcm = 0（AES-256-GCM 认证加密模式）、kSm4Cbc = 1（SM4-CBC 国密块加密模式） |
| 格式常量 | constexpr 定义 | 包含 kDefaultIterations=100000、kSaltLength=16、kGcmIvLength=12、kCbcIvLength=16、kGcmTagLength=16、kAes256KeyLength=32、kSm4KeyLength=16 |
| isValidMagic() | 验证函数 | 检查文件头前 10 字节是否匹配 "BACKERENC" 魔术字 |
| algorithmName() | 转换函数 | Algorithm 枚举值 ↔ 字符串名转换（"aes256" / "sm4"） |

### 密钥派生（KeyDerivation）

| 构件 | 类型 | 说明 |
|------|------|------|
| KeyDerivation | 工具类 | 密钥派生与随机数生成工具类 |
| deriveKey() | 静态方法 | 基于 PBKDF2-HMAC（PKCS5_PBKDF2_HMAC）的密钥派生函数。AES-256 使用 SHA-256 摘要算法；SM4 使用 SM3 国密摘要算法。输出密钥长度：AES 32 字节 / SM4 16 字节 |
| randomBytes() | 静态方法 | 使用 OpenSSL RAND_bytes 生成密码学安全的随机字节序列，用于生成盐值和 IV |

### 接口层

| 构件 | 类型 | 说明 |
|------|------|------|
| Encryptor | 抽象类（接口） | 加密器策略接口，定义 4 个纯虚方法。所有加密算法通过实现此接口接入 |

接口方法规范：

| 方法 | 签名 | 说明 |
|------|------|------|
| encrypt() | `span<char const> input, vector<char>& output, string_view password → Expected<void, ErrorCode>` | 加密 input，将加密数据（含 EncryptedHeader + 密文 + GCM 标签）追加到 output |
| decrypt() | `span<char const> input, vector<char>& output, string_view password → Expected<void, ErrorCode>` | 解密 input（解析 EncryptedHeader → 派生密钥 → 解密），将明文追加到 output |
| name() | `→ string_view` | 返回算法名称："aes256" / "sm4" |
| suffix() | `→ string_view` | 返回加密文件后缀：".enc" |

| 构件 | 类型 | 说明 |
|------|------|------|
| buildEncryptor() | 工厂函数 | 根据算法名称创建 Encryptor 实例。映射："aes256" / "aes-256-gcm" → AES-256-GCM、"sm4" / "sm4-cbc" → SM4-CBC。未知名称返回 nullptr |

### OpenSslEncryptor（OpenSSL 加密实现）

| 构件 | 类型 | 说明 |
|------|------|------|
| OpenSslEncryptor | 实现类 | 基于 OpenSSL EVP 高层 API 的 Encryptor 实现。构造函数接收算法名称字符串，内部解析为 Algorithm 枚举 |
| encrypt() | 成员方法 | 加密流程：① RAND_bytes 生成随机盐 + IV → ② 构造 EncryptedHeader → ③ deriveKey PBKDF2 派生密钥 → ④ EVP_EncryptInit_ex 初始化（GCM 模式需先设置 IV 长度）→ ⑤ EVP_EncryptUpdate 加密块 → ⑥ EVP_EncryptFinal_ex 完成加密 → ⑦ GCM 模式追加 16 字节认证标签 → ⑧ std::fill 清零密钥内存 |
| decrypt() | 成员方法 | 解密流程：① 解析 EncryptedHeader → ② isValidMagic 验证魔术字 → ③ 验证算法匹配 → ④ 提取密文数据（GCM 模式需分离最后 16 字节标签）→ ⑤ deriveKey 派生密钥 → ⑥ EVP_DecryptInit_ex → ⑦ EVP_DecryptUpdate → ⑧ GCM 模式 EVP_CIPHER_CTX_ctrl 设置标签 → ⑨ EVP_DecryptFinal_ex 完成解密（GCM 模式自动验证标签） |
| EVP Encrypt | OpenSSL API 组 | EVP_EncryptInit_ex → EVP_EncryptUpdate → EVP_EncryptFinal_ex 完整加密管线 |
| EVP Decrypt | OpenSSL API 组 | EVP_DecryptInit_ex → EVP_DecryptUpdate → EVP_DecryptFinal_ex 完整解密管线 |

## 加密文件存储格式

```
┌─────────────────────────┐
│  EncryptedHeader        │  47 字节（明文，含算法元数据）
│  (magic + algo + salt   │
│   + iterations + iv)    │
├─────────────────────────┤
│  密文数据               │  变长
├─────────────────────────┤
│  GCM 认证标签           │  16 字节（仅 GCM 模式）
└─────────────────────────┘
```

## 安全特性

1. **密码不落地**：密码仅作为函数参数传递，不存储在文件头中
2. **随机盐值**：每次加密生成 16 字节随机盐，相同密码加密相同数据产生不同密文
3. **高迭代次数**：PBKDF2 默认 100000 次迭代，有效防暴力破解
4. **密钥清零**：加密完成后使用 std::fill 将内存中的密钥材料清零
5. **认证加密**：GCM 模式提供完整性保护，解密时自动验证认证标签，防篡改
6. **国密支持**：SM4-CBC + SM3 密钥派生，满足国产密码合规需求

## 依赖关系

| 依赖 | 说明 |
|------|------|
| OpenSSL (libcrypto) | 提供 EVP 加密 API、RAND_bytes 随机数、PKCS5_PBKDF2_HMAC 密钥派生、SM3/SHA-256 摘要算法 |

## 设计模式

- **策略模式**：Encryptor 接口 + OpenSslEncryptor 实现 + buildEncryptor 工厂
- **模板方法**：encrypt()/decrypt() 定义了加密/解密的标准步骤序列，具体差异（AES vs SM4、GCM vs CBC）通过内部的 cipher()/keyLength()/ivLength()/isGcmMode()/useSm3() 等辅助方法处理
