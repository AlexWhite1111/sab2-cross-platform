# 源码来源与许可

- 本次导入来自 ChatGPT 交接包中的 `SAB2_CrossPlatform_Foundation_v0.1.zip`。外层 ZIP 的 SHA-256 为 `b7a027f662137fcc44ca3e4786c3be592923726b9d0834a383e48f6c55b4d45c`，内层 ZIP 的 SHA-256 为 `7f3a7be684ce5ffda7ca4773442b5ed80a3d79f45ad6be7b0916b21fb1d6e009`。
- 解压后核对了原始 `FILE_MANIFEST.json` 中全部 23 个条目的字节数和 SHA-256。清单用于核对原 ZIP。导入时补充交接和实测记录，应用源码及测试源码保持原始内容。
- v0.1 是参照上游架构编写的独立 C++17 原型。交接包声明未复制上游源码，随包的 [MIT 许可证](LICENSE) 适用于原型。包内无训练模型或第三方库二进制。
- 2026-11-20 复核上游 `main` 为 `03059d5ed2bc2b30f0140b48c01b8992b3e46ffd`。该提交的 [LICENSE](https://github.com/SunOner/sunone_aimbot_2/blob/03059d5ed2bc2b30f0140b48c01b8992b3e46ffd/LICENSE) 为 MIT，版权归 SunOner。后续基于上游源码的迁移应保留上游历史、版权和许可声明。
